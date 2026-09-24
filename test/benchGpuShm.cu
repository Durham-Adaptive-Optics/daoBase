/*
 * benchGpuShm - two-stage GPU pipeline through a host SHM or a GPU SHM.
 * Driven by bench_gpu_shm.py.
 *
 * Stage A (run) produces a W x H float frame on the GPU and hands it to
 * stage B (stageB), another process, which sums each row on the GPU and
 * publishes the H sums in a small host SHM. A times each frame from the start
 * of its GPU work to B's result being published.
 *
 *   benchGpuShm setup  MODE W H DEVICE   create /tmp/bgs_in.im.shm (frame) and /tmp/bgs_out.im.shm
 *   benchGpuShm stageB MODE DEVICE       consumer loop (SIGINT to stop)
 *   benchGpuShm run    MODE FRAMES DEVICE
 *
 * MODE: host      frame in a host SHM: A copies it GPU -> host, B copies it back
 *       gpu       frame in a GPU SHM: A writes it in place, publishes with daoShmCommit
 *       gpu-sync  as gpu, published with daoShmCommitSync (waits for the kernel)
 *
 * run prints "LAT v1 v2 ..." (us) and "CHECK ok|FAIL".
 */
#include <cuda_runtime.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "dao.h"

#define IN  "/tmp/bgs_in.im.shm"
#define OUT "/tmp/bgs_out.im.shm"

static volatile int stop = 0;
static void on_int(int s) { (void) s; stop = 1; }

static double now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec * 1e-3;
}

/* frame value: k + (x % 7), so row sums are known exactly */
__global__ void fill(float *f, int w, int h, int k)
{
    long i = (long) blockIdx.x * blockDim.x + threadIdx.x;
    if (i < (long) w * h)
        f[i] = (float) k + (float) ((i % w) % 7);
}

__global__ void rowsum(const float *f, int w, float *out)
{
    __shared__ float s[256];
    const float *row = f + (long) blockIdx.x * w;
    float acc = 0.f;
    for (int x = threadIdx.x; x < w; x += blockDim.x)
        acc += row[x];
    s[threadIdx.x] = acc;
    __syncthreads();
    for (int n = blockDim.x / 2; n > 0; n /= 2) {
        if (threadIdx.x < n)
            s[threadIdx.x] += s[threadIdx.x + n];
        __syncthreads();
    }
    if (threadIdx.x == 0)
        out[blockIdx.x] = s[0];
}

static float expected_sum(int w, int k)
{
    double s = 0;
    for (int x = 0; x < w; x++)
        s += k + (x % 7);
    return (float) s;
}

static void pin(void *p, size_t n)
{
    if (cudaHostRegister(p, n, cudaHostRegisterPortable) != cudaSuccess)
        cudaGetLastError();                 /* already pinned (libdao) or not pinnable: fine */
}

int main(int argc, char **argv)
{
    daoSetLogLevel(1);
    if (argc < 4)
        return 2;
    const char *role = argv[1], *mode = argv[2];
    int gpu = strcmp(mode, "host") != 0;

    if (!strcmp(role, "setup")) {
        uint32_t w = atoi(argv[3]), h = atoi(argv[4]);
        int dev = atoi(argv[5]);
        uint32_t sin_[2] = { w, h }, sout[2] = { h, 1 };
        IMAGE in, out;
        int rc = gpu ? daoShmCreateGpu(&in, IN, 2, sin_, _DATATYPE_FLOAT, dev, 0, 0)
                     : daoShmCreate(&in, IN, 2, sin_, _DATATYPE_FLOAT, 1, 0);
        rc = rc || daoShmCreate(&out, OUT, 2, sout, _DATATYPE_FLOAT, 1, 0);
        printf(rc ? "SETUP FAILED\n" : "SETUP OK\n");
        return rc;
    }

    int dev = atoi(argv[role[0] == 's' ? 3 : 4]);
    cudaSetDevice(dev);
    cudaSetDeviceFlags(cudaDeviceScheduleSpin);
    cudaStream_t st;
    cudaStreamCreateWithFlags(&st, cudaStreamNonBlocking);
    IMAGE in, out;
    if (daoShmOpen(IN, &in) || daoShmOpen(OUT, &out)) {
        printf("OPEN FAILED\n");
        return 1;
    }
    int w = in.md[0].size[0], h = in.md[0].size[1];
    size_t frame = (size_t) w * h * sizeof(float);
    float *dsum;
    cudaMalloc(&dsum, h * sizeof(float));
    pin(out.array.V, h * sizeof(float));

    if (!strcmp(role, "stageB")) {
        float *dframe = (float *) in.d_array;
        if (!gpu) {
            cudaMalloc(&dframe, frame);
            pin(in.array.V, frame);
        }
        signal(SIGINT, on_int);
        uint64_t seen = in.md[0].cnt0;
        printf("READY\n");
        fflush(stdout);
        while (!stop) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 1;
            if (daoShmWaitSemTimeout(&in, 0, &ts) != DAO_SUCCESS || in.md[0].cnt0 == seen)
                continue;
            seen = in.md[0].cnt0;
            if (!gpu)
                cudaMemcpyAsync(dframe, in.array.V, frame, cudaMemcpyHostToDevice, st);
            rowsum<<<h, 256, 0, st>>>(dframe, w, dsum);
            cudaMemcpyAsync(out.array.V, dsum, h * sizeof(float), cudaMemcpyDeviceToHost, st);
            cudaStreamSynchronize(st);
            daoShmSetDataPartFinalize(&out);
        }
        return 0;
    }

    /* stage A */
    int frames = atoi(argv[3]);
    float *dframe = (float *) in.d_array;
    if (!gpu) {
        cudaMalloc(&dframe, frame);
        pin(in.array.V, frame);
    }
    double *lat = (double *) malloc(sizeof(double) * frames);
    int bad = 0;
    long blocks = ((long) w * h + 255) / 256;
    for (int k = 0; k < frames + 20; k++) {
        uint64_t target = out.md[0].cnt0 + 1;
        double t0 = now_us();
        if (gpu)
            daoShmBeginWrite(&in);
        fill<<<blocks, 256, 0, st>>>(dframe, w, h, k);
        if (!gpu) {                                      /* today: through host memory */
            cudaMemcpyAsync(in.array.V, dframe, frame, cudaMemcpyDeviceToHost, st);
            cudaStreamSynchronize(st);
            daoShmSetDataPartFinalize(&in);
        } else if (!strcmp(mode, "gpu")) {
            daoShmCommit(&in, st);                        /* published when the kernel ends */
        } else {
            daoShmCommitSync(&in, st);                    /* wait for the kernel, publish */
        }
        while (out.md[0].cnt0 < target) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 2;
            if (daoShmWaitSemTimeout(&out, 1, &ts) != DAO_SUCCESS && out.md[0].cnt0 < target) {
                printf("TIMEOUT at frame %d\n", k);
                return 1;
            }
        }
        double t1 = now_us();
        cudaStreamSynchronize(st);                       /* commit callback done before refilling */
        if (k >= 20)
            lat[k - 20] = t1 - t0;
        if (k % 50 == 0) {
            float e = expected_sum(w, k);
            for (int y = 0; y < h; y++)
                if (out.array.F[y] != e)
                    bad++;
        }
    }
    printf("LAT");
    for (int k = 0; k < frames; k++)
        printf(" %.2f", lat[k]);
    printf("\nCHECK %s\n", bad ? "FAIL" : "ok");
    return 0;
}
