/**
 * @file   daoThread.h
 * @brief  Threads, CPU pinning, real-time priority, mutexes and timed waits, the same on
 *         every platform dao builds on.
 *
 * The C programs use these instead of pthread, sched_setscheduler, mlockall and
 * clock_nanosleep, which do not exist on Windows. One implementation per platform:
 * src/c/daoThreadPosix.c (Linux, macOS) and src/c/daoThreadWin32.c (Windows).
 *
 * Every function reports its failures with daoError / daoWarning, so a caller may ignore the
 * return value when there is nothing better to do than go on.
 *
 * The C++ classes (daoThread.hpp: Dao::Thread, ThreadTable) are a separate, higher-level layer.
 *
 * Platform notes
 *  - Linux: everything. Real-time priority: SCHED_FIFO 1..99 (needs CAP_SYS_NICE or an rtprio
 *    limit; the highest allowed is used when the one asked is not, with a warning).
 *  - macOS: no CPU pinning (a warning), real-time priority within what the system allows.
 *  - Windows: a priority above 0 is THREAD_PRIORITY_TIME_CRITICAL in the HIGH priority class
 *    (Windows has a handful of levels, not 99); no whole-process memory locking.
 */
#ifndef DAO_THREAD_H
#define DAO_THREAD_H

#include <stdint.h>
#include "dao.h"                         /* DLL_EXPORT, DAO_SUCCESS / DAO_ERROR; windows.h on Windows */

#ifndef _WIN32
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ threads */

/** A thread (opaque). */
typedef struct daoThread daoThread;

/** A thread's function: the pthread signature, so existing thread functions keep theirs. */
typedef void *(*daoThreadFn)(void *arg);

/** How a thread starts. */
typedef struct {
    int         cpu;          /**< CPU to run on; -1: any */
    int         rtPriority;   /**< 0: as its creator (inherited); 1..99: real-time priority */
    const char *name;         /**< shown by top, ps, debuggers (15 characters on Linux); NULL: none */
} daoThreadAttr;

/** Any CPU, the creator's priority, no name. */
#define DAO_THREAD_ATTR_DEFAULT { -1, 0, NULL }

/**
 * Starts fn(arg) in a new thread.
 * @param t   the new thread, for daoThreadJoin
 * @param a   NULL: DAO_THREAD_ATTR_DEFAULT
 * @return DAO_SUCCESS; DAO_ERROR (said why): not started, *t unchanged. A CPU that does not
 *         exist is an error; a real-time priority not allowed is a warning (see
 *         daoThreadSetRtPriority).
 */
DLL_EXPORT int daoThreadCreate(daoThread **t, const daoThreadAttr *a, daoThreadFn fn, void *arg);

/** Waits for t to end, gives fn's return value (ret may be NULL) and frees t. */
DLL_EXPORT int daoThreadJoin(daoThread *t, void **ret);

/** Moves a thread to one CPU. t NULL: the calling thread. DAO_ERROR (said why) if it could not. */
DLL_EXPORT int daoThreadPin(daoThread *t, int cpu);

/**
 * Real-time priority of a thread. t NULL: the calling thread. priority 0: back to normal.
 * When the priority asked is not allowed, the highest allowed is used, with a warning; when
 * none is, the thread stays at normal priority, with a warning.
 * @return the real-time priority set: priority, a lower one, or 0 (normal).
 */
DLL_EXPORT int daoThreadSetRtPriority(daoThread *t, int priority);

/** The number of CPUs (configured, online or not): CPU numbers go from 0 to this - 1. */
DLL_EXPORT int daoCpuCount(void);

/* ---------------------------------------------------------------- the process */

/**
 * Locks the process's memory, now and to come (mlockall): no page fault in a real-time loop.
 * A warning when not permitted. Linux: needs CAP_IPC_LOCK or a large enough memlock limit;
 * while locked, an allocation beyond that limit fails, so only real-time programs lock.
 */
DLL_EXPORT int daoMemLock(void);

/**
 * A real-time program's start: daoMemLock(), then the calling thread at priority (and on
 * Windows the HIGH priority class and a 1 ms system timer).
 * @return the real-time priority set, as daoThreadSetRtPriority.
 */
DLL_EXPORT int daoProcessRtSetup(int priority);

/* ------------------------------------------------------------------- mutexes */

/** A mutex: a plain struct, so it can be static and initialised with DAO_MUTEX_INIT. */
#ifdef _WIN32
typedef struct { SRWLOCK lock; } daoMutex;
#define DAO_MUTEX_INIT { SRWLOCK_INIT }
#else
typedef struct { pthread_mutex_t m; } daoMutex;
#define DAO_MUTEX_INIT { PTHREAD_MUTEX_INITIALIZER }
#endif

DLL_EXPORT void daoMutexInit(daoMutex *m);       /**< for a mutex not initialised with DAO_MUTEX_INIT */
DLL_EXPORT void daoMutexDestroy(daoMutex *m);
DLL_EXPORT void daoMutexLock(daoMutex *m);
DLL_EXPORT void daoMutexUnlock(daoMutex *m);

/* --------------------------------------------------------------------- time */

/** Monotonic time, ns (not the date: for durations and deadlines). */
DLL_EXPORT int64_t daoTimeNs(void);

/** Sleeps until daoTimeNs() reaches t; at once if it already has. For periodic loops:
 *  t += period; daoSleepUntilNs(t); keeps the period without drift. */
DLL_EXPORT void daoSleepUntilNs(int64_t t);

/** Sleeps dt ns. */
DLL_EXPORT void daoSleepNs(int64_t dt);

#ifdef __cplusplus
}
#endif

#endif /* DAO_THREAD_H */
