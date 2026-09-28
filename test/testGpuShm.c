/*
 * testGpuShm - exercise GPU SHMs from separate processes (see test_gpu_shm.py).
 *
 * The payload is a 64x64 float image whose value i is VALUE + STEP * i
 * (STEP defaults to 1).
 *
 *   testGpuShm create NAME DEVICE MIRROR VALUE exit|hold|crash
 *   testGpuShm big    NAME DEVICE MB      create an MB-sized uint8 GPU SHM and exit
 *   testGpuShm read   NAME VALUE [STEP]   check GetData (host copy) and CopyToHost
 *   testGpuShm host   NAME VALUE [STEP]   check the /tmp host copy only, without GetData
 *   testGpuShm write  NAME VALUE          SetData
 *   testGpuShm wait   NAME SEM CNT0 TIMEOUT_S VALUE [STEP]   wait until cnt0 >= CNT0, then check
 *   testGpuShm info   NAME
 *   testGpuShm consistent NAME COUNT   GetData COUNT times; count frames that are not uniform
 *   testGpuShm big2   NAME DEVICE N    create an N-float, non-mirrored GPU SHM (1-D) and exit
 */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "dao.h"

#define W 64
#define H 64
#define N (W * H)

static float step = 1.f;

static void fill(float *v, float value)
{
    for (int i = 0; i < N; i++)
        v[i] = value + (float) i;
}

static int check(const float *v, float value, const char *what)
{
    for (int i = 0; i < N; i++)
        if (v[i] != value + step * (float) i) {
            printf("MISMATCH %s: [%d] = %g, expected %g\n", what, i, v[i], value + step * (float) i);
            return 0;
        }
    return 1;
}

int main(int argc, char **argv)
{
    static float buf[N];
    IMAGE image;
    memset(&image, 0, sizeof image);
    daoSetLogLevel(1);
    if (argc < 3)
        return 2;
    const char *mode = argv[1], *name = argv[2];

    if (!strcmp(mode, "big")) {
        uint32_t size[2] = { 1024 * 1024, (uint32_t) atoi(argv[4]) };
        if (daoShmCreateGpu(&image, name, 2, size, _DATATYPE_UINT8, atoi(argv[3]), 0, 0) != DAO_SUCCESS)
            return 1;
        printf("OK size=%llu\n", (unsigned long long) image.md[0].gpu_size);
        daoShmClose(&image);
        return 0;
    }
    if (!strcmp(mode, "big2")) {
        uint32_t size[2] = { (uint32_t) atoi(argv[4]), 1 };
        if (daoShmCreateGpu(&image, name, 2, size, _DATATYPE_FLOAT, atoi(argv[3]), 0, 0) != DAO_SUCCESS)
            return 1;
        printf("OK\n");
        daoShmClose(&image);
        return 0;
    }
    if (!strcmp(mode, "create")) {
        uint32_t size[2] = { W, H };
        int device = atoi(argv[3]);
        uint32_t flags = atoi(argv[4]) ? DAO_GPU_MIRROR : 0;
        if (daoShmCreateGpu(&image, name, 2, size, _DATATYPE_FLOAT, device, 0, flags) != DAO_SUCCESS) {
            printf("CREATE FAILED\n");
            return 1;
        }
        fill(buf, (float) atof(argv[5]));
        if (daoShmSetData(&image, buf, N) != DAO_SUCCESS) {
            printf("SETDATA FAILED\n");
            return 1;
        }
        printf("OK id=%llx d_array=%p cnt0=%llu\n", (unsigned long long) image.md[0].gpu_id,
               image.d_array, (unsigned long long) image.md[0].cnt0);
        fflush(stdout);
        if (!strcmp(argv[6], "crash"))
            abort();                                 /* no close, no cleanup */
        if (!strcmp(argv[6], "hold"))
            for (;;)
                pause();
        daoShmClose(&image);
        return 0;
    }

    if (daoShmOpen(name, &image) != DAO_SUCCESS) {
        printf("OPEN FAILED\n");
        return 1;
    }
    if (!strcmp(mode, "info")) {
        printf("gpu=%d device=%d flags=%u size=%llu id=%llx d_array=%p cnt0=%llu\n", daoShmIsGpu(&image),
               image.md[0].gpu_device, image.md[0].gpu_flags, (unsigned long long) image.md[0].gpu_size,
               (unsigned long long) image.md[0].gpu_id, image.d_array, (unsigned long long) image.md[0].cnt0);
    } else if (!strcmp(mode, "read")) {
        float value = (float) atof(argv[3]);
        if (argc > 4)
            step = (float) atof(argv[4]);
        void *p;
        uint32_t idx;
        uint64_t cnt0;
        int ok = daoShmGetData(&image, &p, &idx, &cnt0) == DAO_SUCCESS && check(p, value, "GetData");
        if (image.d_array)
            ok = ok && daoShmCopyToHost(&image, buf, N) == DAO_SUCCESS && check(buf, value, "CopyToHost");
        printf("%s cnt0=%llu d_array=%p\n", ok ? "OK" : "FAIL", (unsigned long long) cnt0, image.d_array);
        daoShmClose(&image);
        return ok ? 0 : 1;
    } else if (!strcmp(mode, "host")) {
        if (argc > 4)
            step = (float) atof(argv[4]);
        int ok = check(image.array.F, (float) atof(argv[3]), "host copy");
        printf("%s\n", ok ? "OK" : "FAIL");
        daoShmClose(&image);
        return ok ? 0 : 1;
    } else if (!strcmp(mode, "write")) {
        fill(buf, (float) atof(argv[3]));
        int ok = daoShmSetData(&image, buf, N) == DAO_SUCCESS;
        printf("%s cnt0=%llu\n", ok ? "OK" : "FAIL", (unsigned long long) image.md[0].cnt0);
        daoShmClose(&image);
        return ok ? 0 : 1;
    } else if (!strcmp(mode, "consistent")) {
        int count = atoi(argv[3]), torn = 0;
        long n = (long) image.md[0].nelement;
        struct timespec start, now;
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (int c = 0; c < count; c++) {
            void *p;
            uint32_t idx;
            uint64_t cnt;
            if (daoShmGetData(&image, &p, &idx, &cnt) != DAO_SUCCESS)
                return 1;
            const float *v = (const float *) p;
            for (long i = 1; i < n; i++)
                if (v[i] != v[0]) {
                    torn++;
                    break;
                }
            usleep(rand() % 700);
        }
        clock_gettime(CLOCK_MONOTONIC, &now);
        printf("TORN %d/%d in %.2f s\n", torn, count,
               now.tv_sec - start.tv_sec + 1e-9 * (now.tv_nsec - start.tv_nsec));
        daoShmClose(&image);
        return 0;
    } else if (!strcmp(mode, "wait")) {
        int sem = atoi(argv[3]);
        uint64_t target = strtoull(argv[4], NULL, 10);
        double timeout = atof(argv[5]);
        float value = (float) atof(argv[6]);
        struct timespec start, now;
        if (argc > 7)
            step = (float) atof(argv[7]);
        clock_gettime(CLOCK_MONOTONIC, &start);
        printf("READY\n");
        fflush(stdout);
        while (image.md[0].cnt0 < target) {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 100 * 1000 * 1000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            daoShmWaitSemTimeout(&image, sem, &ts);
            clock_gettime(CLOCK_MONOTONIC, &now);
            if (now.tv_sec - start.tv_sec + 1e-9 * (now.tv_nsec - start.tv_nsec) > timeout) {
                printf("TIMEOUT cnt0=%llu\n", (unsigned long long) image.md[0].cnt0);
                return 1;
            }
        }
        void *p;
        uint32_t idx;
        uint64_t cnt0;
        int ok = daoShmGetData(&image, &p, &idx, &cnt0) == DAO_SUCCESS && check(p, value, "after wait");
        printf("%s cnt0=%llu\n", ok ? "OK" : "FAIL", (unsigned long long) cnt0);
        daoShmClose(&image);
        return ok ? 0 : 1;
    }
    return 2;
}
