/**
 * @file   daoThreadWin32.c
 * @brief  daoThread.h on Windows (_beginthreadex, thread affinity and priority, SRW locks,
 *         high-resolution waitable timers).
 *
 * Real-time priority: Windows has a few levels within a priority class, not 99. Any
 * priority above 0 gives THREAD_PRIORITY_TIME_CRITICAL, with the process in the HIGH
 * priority class (REALTIME needs administrator rights and can starve the system's own
 * threads). There is no whole-process memory lock (mlockall).
 */
#include <errno.h>
#include <process.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <mmsystem.h>                     /* timeBeginPeriod */

#include "daoThread.h"

/* dao.h's daoLogLevel is a copy per source file: follow the level set with daoSetLogLevel */
#define daoLogLevel daoGetLogLevel()

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

#pragma comment(lib, "winmm.lib")         /* timeBeginPeriod (MSVC; wscript_build adds it too) */

struct daoThread {
    HANDLE      h;
    daoThreadFn fn;
    void       *arg;
    void       *ret;
    int         cpu;
    int         rtPriority;
    char        name[64];
};

/* ------------------------------------------------------------------ threads */

typedef HRESULT (WINAPI *SetThreadDescriptionFn)(HANDLE, PCWSTR);

static void set_name(const char *name)
{
    /* SetThreadDescription: Windows 10 1607 and later; looked up so older systems still run */
    SetThreadDescriptionFn f = (SetThreadDescriptionFn)
        (void (*)(void)) GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription");
    WCHAR w[64];
    if (!f || !MultiByteToWideChar(CP_UTF8, 0, name, -1, w, 64))
        return;
    f(GetCurrentThread(), w);
}

static unsigned __stdcall trampoline(void *p)
{
    daoThread *t = (daoThread *) p;
    if (t->name[0])
        set_name(t->name);
    if (t->cpu >= 0)
        daoThreadPin(NULL, t->cpu);
    if (t->rtPriority > 0)
        daoThreadSetRtPriority(NULL, t->rtPriority);
    t->ret = t->fn(t->arg);
    return 0;
}

int daoThreadCreate(daoThread **out, const daoThreadAttr *a, daoThreadFn fn, void *arg)
{
    daoThreadAttr def = DAO_THREAD_ATTR_DEFAULT;
    daoThread *t;
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
    if (a->name)
        strncpy(t->name, a->name, sizeof t->name - 1);
    if (a->cpu >= 0 && a->cpu >= daoCpuCount()) {
        daoError("daoThreadCreate(%s): no CPU %d (this machine has %d)\n", t->name, a->cpu, daoCpuCount());
        free(t);
        return DAO_ERROR;
    }
    /* _beginthreadex, not CreateThread: the C runtime is set up for the new thread */
    t->h = (HANDLE) _beginthreadex(NULL, 0, trampoline, t, 0, NULL);
    if (!t->h) {
        daoError("daoThreadCreate(%s): _beginthreadex failed (%d)\n", t->name, errno);
        free(t);
        return DAO_ERROR;
    }
    *out = t;
    return DAO_SUCCESS;
}

int daoThreadJoin(daoThread *t, void **ret)
{
    if (!t)
        return DAO_ERROR;
    if (WaitForSingleObject(t->h, INFINITE) != WAIT_OBJECT_0) {
        daoError("daoThreadJoin(%s): WaitForSingleObject failed (%lu)\n", t->name, GetLastError());
        return DAO_ERROR;
    }
    CloseHandle(t->h);
    if (ret)
        *ret = t->ret;
    free(t);
    return DAO_SUCCESS;
}

int daoThreadPin(daoThread *t, int cpu)
{
    HANDLE h = t ? t->h : GetCurrentThread();
    WORD groups = GetActiveProcessorGroupCount(), g;
    int left = cpu;
    if (cpu < 0) {
        daoError("daoThreadPin: no CPU %d\n", cpu);
        return DAO_ERROR;
    }
    /* CPU numbers run through the processor groups (64 CPUs at most each) */
    for (g = 0; g < groups; g++) {
        int n = (int) GetActiveProcessorCount(g);
        if (left < n) {
            GROUP_AFFINITY ga;
            memset(&ga, 0, sizeof ga);
            ga.Group = g;
            ga.Mask = (KAFFINITY) 1 << left;
            if (!SetThreadGroupAffinity(h, &ga, NULL)) {
                daoError("daoThreadPin(cpu=%d): SetThreadGroupAffinity failed (%lu)\n", cpu, GetLastError());
                return DAO_ERROR;
            }
            return DAO_SUCCESS;
        }
        left -= n;
    }
    daoError("daoThreadPin: no CPU %d (this machine has %d)\n", cpu, daoCpuCount());
    return DAO_ERROR;
}

static void high_class(void)
{
    static LONG done = 0;
    if (InterlockedExchange(&done, 1) == 0 && !SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        daoWarning("SetPriorityClass(HIGH) failed (%lu)\n", GetLastError());
}

int daoThreadSetRtPriority(daoThread *t, int priority)
{
    HANDLE h = t ? t->h : GetCurrentThread();
    if (priority <= 0) {
        SetThreadPriority(h, THREAD_PRIORITY_NORMAL);
        return 0;
    }
    high_class();                          /* TIME_CRITICAL means little in the NORMAL class */
    if (!SetThreadPriority(h, THREAD_PRIORITY_TIME_CRITICAL)) {
        daoWarning("SetThreadPriority(TIME_CRITICAL) failed (%lu): running at normal priority\n", GetLastError());
        return 0;
    }
    daoInfo("THREAD_PRIORITY_TIME_CRITICAL set (priority %d asked)\n", priority);
    return priority;
}

int daoCpuCount(void)
{
    DWORD n = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    return n > 0 ? (int) n : 1;
}

/* ---------------------------------------------------------------- the process */

int daoMemLock(void)
{
    static int warned = 0;
    if (!warned) {
        warned = 1;
        daoWarning("daoMemLock: Windows has no whole-process memory lock (mlockall): ignored\n");
    }
    return DAO_ERROR;
}

int daoProcessRtSetup(int priority)
{
    daoMemLock();
    if (priority > 0) {
        high_class();
        timeBeginPeriod(1);                /* 1 ms system timer: shorter waits for everything */
    }
    return daoThreadSetRtPriority(NULL, priority);
}

/* ------------------------------------------------------------------- mutexes */

void daoMutexInit(daoMutex *m)    { InitializeSRWLock(&m->lock); }
void daoMutexDestroy(daoMutex *m) { (void) m; }    /* an SRW lock holds no resource */
void daoMutexLock(daoMutex *m)    { AcquireSRWLockExclusive(&m->lock); }
void daoMutexUnlock(daoMutex *m)  { ReleaseSRWLockExclusive(&m->lock); }

/* --------------------------------------------------------------------- time */

int64_t daoTimeNs(void)
{
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (!f.QuadPart)
        QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    /* without overflowing: whole seconds, then the rest */
    return (c.QuadPart / f.QuadPart) * 1000000000LL + (c.QuadPart % f.QuadPart) * 1000000000LL / f.QuadPart;
}

void daoSleepUntilNs(int64_t t)
{
    static __declspec(thread) HANDLE timer = NULL;   /* one per thread, made at its first wait */
    int64_t dt = t - daoTimeNs();
    LARGE_INTEGER due;
    if (dt <= 0)
        return;
    if (!timer) {
        timer = CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!timer)                            /* before Windows 10 1803: a normal timer */
            timer = CreateWaitableTimerW(NULL, TRUE, NULL);
    }
    if (!timer) {
        Sleep((DWORD) (dt / 1000000));
        return;
    }
    due.QuadPart = -(dt / 100);                 /* relative, 100 ns units */
    if (due.QuadPart == 0)
        due.QuadPart = -1;
    if (SetWaitableTimer(timer, &due, 0, NULL, NULL, FALSE))
        WaitForSingleObject(timer, INFINITE);
}

void daoSleepNs(int64_t dt)
{
    if (dt > 0)
        daoSleepUntilNs(daoTimeNs() + dt);
}
