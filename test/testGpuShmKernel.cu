/*
 * testGpuShmKernel - CUDA kernels working directly on GPU SHMs (see test_gpu_shm.py).
 *
 *   testGpuShmKernel scale    NAME FACTOR   d_array *= FACTOR in a kernel, then daoShmCommit
 *   testGpuShmKernel device   NAME VALUE    fill a cudaMalloc buffer, daoShmSetDataDevice
 *   testGpuShmKernel pipeline IN OUT FRAMES for FRAMES updates of IN: OUT = 2 * IN on the GPU
 *
 * Built with nvcc against libdao; uses the CUDA runtime, whose primary context
 * is the one libdao maps GPU SHMs into.
 */
#include <cuda_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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

    if (!strcmp(argv[1], "scale")) {
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
