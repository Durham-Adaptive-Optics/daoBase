/**
 * @file   daoThreadPosix.c
 * @brief  daoThread.h on Linux and macOS (pthread, sched, mlockall, clock_nanosleep).
 */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE                       /* pthread_setaffinity_np, pthread_setname_np, CPU_SET */
#endif

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include "daoThread.h"

/* dao.h's daoLogLevel is a copy per source file: follow the level set with daoSetLogLevel */
#define daoLogLevel daoGetLogLevel()

struct daoThread {
    pthread_t   th;
    daoThreadFn fn;
    void       *arg;
    int         cpu;
    int         rtPriority;
    char        name[16];
};

/* ------------------------------------------------------------------ threads */

static void *trampoline(void *p)
{
    daoThread *t = (daoThread *) p;
    if (t->name[0]) {
#ifdef __APPLE__
        pthread_setname_np(t->name);      /* macOS: the calling thread only */
#else
        pthread_setname_np(pthread_self(), t->name);
#endif
    }
#ifdef __APPLE__
    if (t->cpu >= 0)
        daoThreadPin(NULL, t->cpu);       /* warns: no pinning on macOS */
#endif
    if (t->rtPriority > 0)
        daoThreadSetRtPriority(NULL, t->rtPriority);
    return t->fn(t->arg);
}

int daoThreadCreate(daoThread **out, const daoThreadAttr *a, daoThreadFn fn, void *arg)
{
    daoThreadAttr def = DAO_THREAD_ATTR_DEFAULT;
    pthread_attr_t attr;
    daoThread *t;
    int rc;

    if (!out || !fn) {
        daoError("daoThreadCreate: no thread or no function\n");
        return DAO_ERROR;
    }
    if (!a)
        a = &def;
    t = (daoThread *) calloc(1, sizeof *t);
    if (!t) {
        daoError("daoThreadCreate: out of memory\n");
        return DAO_ERROR;
    }
    t->fn = fn;
    t->arg = arg;
    t->cpu = a->cpu;
    t->rtPriority = a->rtPriority;
    if (a->name) {
        strncpy(t->name, a->name, sizeof t->name - 1);
    }
    if (a->cpu >= 0 && a->cpu >= daoCpuCount()) {
        daoError("daoThreadCreate(%s): no CPU %d (this machine has %d)\n", t->name, a->cpu, daoCpuCount());
        free(t);
        return DAO_ERROR;
    }
    pthread_attr_init(&attr);              /* the creator's scheduling is inherited */
#ifdef __linux__
    if (a->cpu >= 0) {                      /* pinned from its first instruction */
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(a->cpu, &set);
        pthread_attr_setaffinity_np(&attr, sizeof set, &set);
    }
#endif
    rc = pthread_create(&t->th, &attr, trampoline, t);
    pthread_attr_destroy(&attr);
    if (rc != 0) {
        daoError("daoThreadCreate(%s): %s%s\n", t->name, strerror(rc),
                 a->cpu >= 0 ? " (is the CPU online and allowed to this process?)" : "");
        free(t);
        return DAO_ERROR;
    }
    *out = t;
    return DAO_SUCCESS;
}

int daoThreadJoin(daoThread *t, void **ret)
{
    void *r = NULL;
    int rc;
    if (!t)
        return DAO_ERROR;
    rc = pthread_join(t->th, &r);
    if (rc != 0) {
        daoError("daoThreadJoin(%s): %s\n", t->name, strerror(rc));
        return DAO_ERROR;
    }
    if (ret)
        *ret = r;
    free(t);
    return DAO_SUCCESS;
}

int daoThreadPin(daoThread *t, int cpu)
{
#ifdef __linux__
    cpu_set_t set;
    int rc;
    if (cpu < 0 || cpu >= daoCpuCount() || cpu >= CPU_SETSIZE) {
        daoError("daoThreadPin: no CPU %d (this machine has %d)\n", cpu, daoCpuCount());
        return DAO_ERROR;
    }
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    rc = pthread_setaffinity_np(t ? t->th : pthread_self(), sizeof set, &set);
    if (rc != 0) {
        daoError("daoThreadPin(cpu=%d): %s\n", cpu, strerror(rc));
        return DAO_ERROR;
    }
    return DAO_SUCCESS;
#else
    static int warned = 0;
    (void) t;
    if (!warned) {
        warned = 1;
        daoWarning("daoThreadPin(cpu=%d): CPU pinning is not available on this system: ignored\n", cpu);
    }
    return DAO_ERROR;
#endif
}

int daoThreadSetRtPriority(daoThread *t, int priority)
{
    pthread_t th = t ? t->th : pthread_self();
    struct sched_param sp;
    int rc, lo, hi;

    memset(&sp, 0, sizeof sp);
    if (priority <= 0) {
        rc = pthread_setschedparam(th, SCHED_OTHER, &sp);
        if (rc != 0)
            daoWarning("daoThreadSetRtPriority(0): %s\n", strerror(rc));
        return 0;
    }
    lo = sched_get_priority_min(SCHED_FIFO);
    hi = sched_get_priority_max(SCHED_FIFO);
    if (priority > hi) {
        daoWarning("SCHED_FIFO priority %d above this system's %d: %d used\n", priority, hi, hi);
        priority = hi;
    }
    if (priority < lo)
        priority = lo;
    sp.sched_priority = priority;
    rc = pthread_setschedparam(th, SCHED_FIFO, &sp);
    if (rc == 0) {
        daoInfo("SCHED_FIFO priority %d set\n", priority);
        return priority;
    }
#ifdef RLIMIT_RTPRIO
    {
        /* not permitted at this priority: the highest this user's rtprio limit allows */
        struct rlimit rl;
        int allowed = getrlimit(RLIMIT_RTPRIO, &rl) == 0 ? (int) rl.rlim_cur : 0;
        if (allowed >= lo && allowed > 0 && allowed < priority) {
            sp.sched_priority = allowed;
            if (pthread_setschedparam(th, SCHED_FIFO, &sp) == 0) {
                daoWarning("SCHED_FIFO priority %d not permitted (this user's rtprio limit is %d): "
                           "running at %d. Raise the rtprio limit (/etc/security/limits.conf) or grant "
                           "CAP_SYS_NICE for full priority.\n", priority, allowed, allowed);
                return allowed;
            }
        }
    }
    daoWarning("SCHED_FIFO priority %d failed (%s): running at normal, non-real-time priority. "
               "Raise this user's rtprio limit (/etc/security/limits.conf) or grant CAP_SYS_NICE.\n",
               priority, strerror(rc));
#else
    daoWarning("SCHED_FIFO priority %d failed (%s): running at normal, non-real-time priority\n",
               priority, strerror(rc));
#endif
    return 0;
}

int daoCpuCount(void)
{
    long n = sysconf(_SC_NPROCESSORS_CONF);
    return n > 0 ? (int) n : 1;
}

/* ---------------------------------------------------------------- the process */

int daoMemLock(void)
{
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        daoWarning("mlockall failed (%s): memory not locked, page faults possible in the loop. "
                   "Grant CAP_IPC_LOCK or raise the memlock limit (daoTools: scripts/daoToolSetCap).\n",
                   strerror(errno));
        return DAO_ERROR;
    }
    return DAO_SUCCESS;
}

int daoProcessRtSetup(int priority)
{
    daoMemLock();
    return daoThreadSetRtPriority(NULL, priority);
}

/* ------------------------------------------------------------------- mutexes */

void daoMutexInit(daoMutex *m)    { pthread_mutex_init(&m->m, NULL); }
void daoMutexDestroy(daoMutex *m) { pthread_mutex_destroy(&m->m); }
void daoMutexLock(daoMutex *m)    { pthread_mutex_lock(&m->m); }
void daoMutexUnlock(daoMutex *m)  { pthread_mutex_unlock(&m->m); }

/* --------------------------------------------------------------------- time */

int64_t daoTimeNs(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t) ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void daoSleepUntilNs(int64_t t)
{
#ifdef __APPLE__
    /* no clock_nanosleep: a relative sleep to the deadline */
    int64_t dt;
    while ((dt = t - daoTimeNs()) > 0) {
        struct timespec d = { (time_t) (dt / 1000000000LL), (long) (dt % 1000000000LL) };
        nanosleep(&d, NULL);
    }
#else
    struct timespec ts = { (time_t) (t / 1000000000LL), (long) (t % 1000000000LL) };
    while (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) == EINTR)
        ;
#endif
}

void daoSleepNs(int64_t dt)
{
    if (dt > 0)
        daoSleepUntilNs(daoTimeNs() + dt);
}
