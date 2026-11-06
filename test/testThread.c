/*
 * testThread: daoThread.h. Threads and their return values, a mutex under contention, CPU
 * pinning, a CPU that does not exist, real-time priority (reported: it needs rights this
 * machine may not give), periodic absolute waits.
 *
 *     build/test/testThread        (exit code = failures)
 */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "daoThread.h"

#ifdef __linux__
#include <sched.h>
#endif

static int failures = 0;
#define CHECK(cond, ...) do { int ok_ = (cond); printf("%-58s %s\n", __VA_ARGS__, ok_ ? "PASS" : "FAIL"); failures += !ok_; } while (0)

/* --- a mutex under contention */
static daoMutex gMutex = DAO_MUTEX_INIT;
static long gCount = 0;

static void *adder(void *arg)
{
    long n = (long) (intptr_t) arg;
    for (long i = 0; i < n; i++) {
        daoMutexLock(&gMutex);
        gCount++;
        daoMutexUnlock(&gMutex);
    }
    return (void *) (intptr_t) n;
}

/* --- where a thread runs, at which priority */
typedef struct { int cpu; int prio; char name[32]; } Where;

static void *where(void *arg)
{
    Where *w = (Where *) arg;
#ifdef __linux__
    struct sched_param sp;
    int policy;
    w->cpu = sched_getcpu();
    pthread_getschedparam(pthread_self(), &policy, &sp);
    w->prio = policy == SCHED_FIFO ? sp.sched_priority : 0;
    pthread_getname_np(pthread_self(), w->name, sizeof w->name);
#else
    w->cpu = -1;
    w->prio = -1;
#endif
    return NULL;
}

int main(void)
{
    int ncpu = daoCpuCount();
    printf("daoCpuCount: %d\n", ncpu);
    CHECK(ncpu >= 1, "CPU count");

    /* 4 threads, a shared counter, return values */
    {
        daoThread *t[4];
        long n = 200000, sum = 0;
        int ok = 1;
        for (int i = 0; i < 4; i++)
            ok &= daoThreadCreate(&t[i], NULL, adder, (void *) (intptr_t) n) == DAO_SUCCESS;
        for (int i = 0; i < 4; i++) {
            void *r = NULL;
            ok &= daoThreadJoin(t[i], &r) == DAO_SUCCESS;
            sum += (long) (intptr_t) r;
        }
        CHECK(ok && gCount == 4 * n && sum == 4 * n, "4 threads, mutex: count and return values");
    }

    /* a mutex made at run time */
    {
        daoMutex m;
        daoMutexInit(&m);
        daoMutexLock(&m);
        daoMutexUnlock(&m);
        daoMutexDestroy(&m);
        CHECK(1, "daoMutexInit / Lock / Unlock / Destroy");
    }

    /* pinned and named from the start */
    {
        int cpu = ncpu - 1;
        daoThreadAttr a = DAO_THREAD_ATTR_DEFAULT;
        Where w;
        daoThread *t;
        memset(&w, 0, sizeof w);
        a.cpu = cpu;
        a.name = "daoTestPinned";
        int ok = daoThreadCreate(&t, &a, where, &w) == DAO_SUCCESS && daoThreadJoin(t, NULL) == DAO_SUCCESS;
#ifdef __linux__
        printf("  ran on CPU %d (asked %d), name '%s'\n", w.cpu, cpu, w.name);
        CHECK(ok && w.cpu == cpu && strcmp(w.name, "daoTestPinned") == 0, "pinned + named thread");
#else
        CHECK(ok, "pinned + named thread (pinning not checked here)");
#endif
    }

    /* a CPU that does not exist: refused, nothing started */
    {
        daoThreadAttr a = DAO_THREAD_ATTR_DEFAULT;
        daoThread *t = NULL;
        Where w;
        a.cpu = ncpu + 4;
        CHECK(daoThreadCreate(&t, &a, where, &w) == DAO_ERROR && t == NULL, "no such CPU: refused");
    }

    /* pinning the calling thread, then a CPU that does not exist */
    CHECK(daoThreadPin(NULL, 0) == DAO_SUCCESS, "pin the calling thread to CPU 0");
    CHECK(daoThreadPin(NULL, ncpu + 4) == DAO_ERROR, "pin to no such CPU: refused");

    /* real-time priority: reported, not required (needs CAP_SYS_NICE or an rtprio limit) */
    {
        daoThreadAttr a = DAO_THREAD_ATTR_DEFAULT;
        Where w;
        daoThread *t;
        memset(&w, 0, sizeof w);
        a.rtPriority = 10;
        int ok = daoThreadCreate(&t, &a, where, &w) == DAO_SUCCESS && daoThreadJoin(t, NULL) == DAO_SUCCESS;
        printf("  thread asked for SCHED_FIFO 10: got %d%s\n", w.prio, w.prio ? "" : " (no rights here: normal priority, warned)");
        CHECK(ok, "thread with a real-time priority (started)");
        int got = daoThreadSetRtPriority(NULL, 5);
        printf("  calling thread asked for 5: got %d\n", got);
        CHECK(got == 0 || got <= 5, "daoThreadSetRtPriority returns what it set");
        CHECK(daoThreadSetRtPriority(NULL, 0) == 0, "back to normal priority");
    }

    /* periodic absolute waits: never early, little late */
    {
        const int n = 200;
        const int64_t period = 1000000;            /* 1 ms */
        int64_t t = daoTimeNs(), worst = 0, early = 0, sum = 0;
        for (int i = 0; i < n; i++) {
            t += period;
            daoSleepUntilNs(t);
            int64_t late = daoTimeNs() - t;
            if (late < 0) early++;
            if (late > worst) worst = late;
            sum += late;
        }
        printf("  %d x 1 ms: late %.1f us on average, %.1f us at worst, %lld early\n",
               n, sum / 1e3 / n, worst / 1e3, (long long) early);
        CHECK(early == 0, "daoSleepUntilNs never wakes early");
        int64_t t0 = daoTimeNs();
        daoSleepNs(2000000);
        int64_t dt = daoTimeNs() - t0;
        CHECK(dt >= 2000000 && dt < 50000000, "daoSleepNs(2 ms)");
        daoSleepUntilNs(daoTimeNs() - 1000);        /* in the past: returns at once */
        CHECK(1, "deadline in the past");
    }

    printf("%s\n", failures ? "FAILED" : "ALL PASSED");
    return failures;
}
