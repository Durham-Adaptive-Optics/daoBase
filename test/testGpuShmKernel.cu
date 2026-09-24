/*
 * testGpuShmKernel - CUDA kernels working directly on GPU SHMs (see test_gpu_shm.py).
 *
 *   testGpuShmKernel scale    NAME FACTOR   d_array *= FACTOR in a kernel, then daoShmCommit
 *   testGpuShmKernel device   NAME VALUE    fill a cudaMalloc buffer, daoShmSetDataDevice
 *   testGpuShmKernel pipeline IN OUT FRAMES for FRAMES updates of IN: OUT = 2 * IN on the GPU
 *   testGpuShmKernel scalesync NAME FACTOR  as scale, published with daoShmCommitSync
 *   testGpuShmKernel writer   NAME FRAMES PERIOD_US mark|nomark
 *                             uniform frames (all values = frame number) written by a slow
 *                             kernel; "mark": daoShmBeginWrite + daoShmCommitSync, "nomark":
 *                             synchronise and publish without marking the write
 *   testGpuShmKernel begin-abort NAME       mark a write, then crash
 *
 * Built with nvcc against libdao; uses the CUDA runtime, whose primary context
 * is the one libdao maps GPU SHMs into.
 */
#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "dao.h"

#define CK(x) do { cudaError_t e_ = (x); if (e_) { printf("%s: %s\n", #x, cudaGetErrorString(e_)); return 1; } } while (0)

__global__ void scale(float *p, float f, int n)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        p[i] *= f;
}

__global__ void ramp(float *p, float v, int n)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        p[i] = v + (float) i;
}

/* every value = v, blocks spread over ~1 ms so that a reader can land mid-write */
__global__ void slowfill(float *p, float v, int n)
{
    long long start = clock64(), wait = (long long) blockIdx.x * 1000;
    while (clock64() - start < wait)
        ;
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        p[i] = v;
}

__global__ void twice(const float *in, float *out, int n)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n)
        out[i] = 2.f * in[i];
}

int main(int argc, char **argv)
{
    IMAGE a, b;
    cudaStream_t stream;
    memset(&a, 0, sizeof a);
    memset(&b, 0, sizeof b);
    daoSetLogLevel(1);
    if (argc < 4)
        return 2;
    if (daoShmOpen(argv[2], &a) != DAO_SUCCESS || !a.d_array) {
        printf("OPEN FAILED\n");
        return 1;
    }
    int n = (int) a.md[0].nelement, blocks = (n + 255) / 256;
    CK(cudaStreamCreate(&stream));

    if (!strcmp(argv[1], "begin-abort")) {
        daoShmBeginWrite(&a);
        abort();
    }
    if (!strcmp(argv[1], "scalesync")) {
        daoShmBeginWrite(&a);
        scale<<<blocks, 256, 0, stream>>>((float *) a.d_array, (float) atof(argv[3]), n);
        if (daoShmCommitSync(&a, stream) != DAO_SUCCESS)
            return 1;
        printf("OK cnt0=%llu\n", (unsigned long long) a.md[0].cnt0);
    } else if (!strcmp(argv[1], "writer")) {
        int frames = atoi(argv[3]), period = atoi(argv[4]), mark = !strcmp(argv[5], "mark");
        int wb = (n + 1023) / 1024;
        printf("READY\n");
        fflush(stdout);
        for (int k = 1; k <= frames; k++) {
            if (mark)
                daoShmBeginWrite(&a);
            slowfill<<<wb, 1024, 0, stream>>>((float *) a.d_array, (float) k, n);
            if (mark)
                daoShmCommitSync(&a, stream);
            else {
                CK(cudaStreamSynchronize(stream));
                daoShmSetDataPartFinalize(&a);
            }
            usleep(period);
        }
        printf("OK frames=%d\n", frames);
    } else if (!strcmp(argv[1], "scale")) {
        scale<<<blocks, 256, 0, stream>>>((float *) a.d_array, (float) atof(argv[3]), n);
        CK(cudaGetLastError());
        if (daoShmCommit(&a, stream) != DAO_SUCCESS)
            return 1;
        CK(cudaStreamSynchronize(stream));      /* the commit callback has run */
        printf("OK cnt0=%llu\n", (unsigned long long) a.md[0].cnt0);
    } else if (!strcmp(argv[1], "device")) {
        float *d;
        CK(cudaMalloc(&d, n * sizeof(float)));
        ramp<<<blocks, 256, 0, stream>>>(d, (float) atof(argv[3]), n);
        if (daoShmSetDataDevice(&a, d, n, stream) != DAO_SUCCESS)
            return 1;
        CK(cudaStreamSynchronize(stream));
        CK(cudaFree(d));
        printf("OK cnt0=%llu\n", (unsigned long long) a.md[0].cnt0);
    } else if (!strcmp(argv[1], "pipeline")) {
        int frames = atoi(argv[4]), done = 0;
        if (daoShmOpen(argv[3], &b) != DAO_SUCCESS || !b.d_array) {
            printf("OPEN OUT FAILED\n");
            return 1;
        }
        uint64_t seen = a.md[0].cnt0;
        printf("READY\n");
        fflush(stdout);
        while (done < frames) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 5;
            if (daoShmWaitSemTimeout(&a, 3, &ts) != DAO_SUCCESS) {
                printf("TIMEOUT after %d frames\n", done);
                return 1;
            }
            if (a.md[0].cnt0 == seen)
                continue;
            seen = a.md[0].cnt0;
            twice<<<blocks, 256, 0, stream>>>((const float *) a.d_array, (float *) b.d_array, n);
            if (daoShmCommit(&b, stream) != DAO_SUCCESS)
                return 1;
            done++;
        }
        CK(cudaStreamSynchronize(stream));
        printf("OK frames=%d out_cnt0=%llu\n", done, (unsigned long long) b.md[0].cnt0);
        daoShmClose(&b);
    }
    daoShmClose(&a);
    return 0;
}
