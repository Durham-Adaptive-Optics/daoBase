/**
 * @file    daoNetPlatform.h
 * @brief   The little of the operating system daoShmNetd and daoNetResolve need,
 *          one way on Linux, macOS and Windows: sockets, threads, locks, clocks,
 *          directory listing. Header only (static functions), internal to daoBase.
 */
#ifndef DAO_NET_PLATFORM_H
#define DAO_NET_PLATFORM_H

/* Linux: getaddrinfo, nanosleep, ... also under a strict -std=c11. Include this
 * header first, before any system header. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#  define _GNU_SOURCE
#endif

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <windows.h>
#  include <io.h>
typedef SOCKET dn_sock;
#  define DN_INVALID INVALID_SOCKET
#  define DN_PATHSEP '\\'
#else
#  include <arpa/inet.h>
#  include <dirent.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <pthread.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/uio.h>
#  include <sys/time.h>
#  include <unistd.h>
typedef int dn_sock;
#  define DN_INVALID (-1)
#  define DN_PATHSEP '/'
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ------------------------------------------------------------------ sockets */

static inline int dn_net_init(void)
{
#ifdef _WIN32
    WSADATA w;
    return WSAStartup(MAKEWORD(2, 2), &w) == 0 ? 0 : -1;
#else
    return 0;
#endif
}

static inline void dn_close(dn_sock s)
{
    if (s == DN_INVALID)
        return;
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

static inline void dn_shutdown(dn_sock s)
{
    if (s == DN_INVALID)
        return;
#ifdef _WIN32
    shutdown(s, SD_BOTH);
#else
    shutdown(s, SHUT_RDWR);
#endif
}

/** Low latency: no Nagle, and big buffers for big frames. */
static inline void dn_tune(dn_sock s)
{
    int one = 1, buf = 8 * 1024 * 1024;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *) &one, sizeof one);
    setsockopt(s, SOL_SOCKET, SO_SNDBUF, (const char *) &buf, sizeof buf);
    setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char *) &buf, sizeof buf);
#ifdef SO_NOSIGPIPE
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (const char *) &one, sizeof one);   /* macOS */
#endif
}

/** A send timeout (ms, 0: none): a send to a reader that stopped reading fails after it. */
static inline void dn_send_timeout(dn_sock s, int ms)
{
#ifdef _WIN32
    DWORD t = (DWORD) ms;
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *) &t, sizeof t);
#else
    struct timeval t;
    t.tv_sec = ms / 1000;
    t.tv_usec = (ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof t);
#endif
}

/** A receive timeout (ms, 0: none): a blocking recv then fails after it. */
static inline void dn_recv_timeout(dn_sock s, int ms)
{
#ifdef _WIN32
    DWORD t = (DWORD) ms;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *) &t, sizeof t);
#else
    struct timeval t;
    t.tv_sec = ms / 1000;
    t.tv_usec = (ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof t);
#endif
}

#ifndef MSG_NOSIGNAL
#  define MSG_NOSIGNAL 0
#endif

/** All of buf, or -1. */
static inline int dn_send_all(dn_sock s, const void *buf, size_t len)
{
    const char *p = (const char *) buf;
    while (len > 0) {
        int chunk = len > (1u << 30) ? (1 << 30) : (int) len;
        int n = (int) send(s, p, chunk, MSG_NOSIGNAL);
        if (n <= 0) {
#ifndef _WIN32
            if (n < 0 && errno == EINTR)
                continue;
#endif
            return -1;
        }
        p += n;
        len -= (size_t) n;
    }
    return 0;
}

/** Exactly len bytes into buf, or -1 (closed, error, or the receive timeout). */
static inline int dn_recv_all(dn_sock s, void *buf, size_t len)
{
    char *p = (char *) buf;
    while (len > 0) {
        int chunk = len > (1u << 30) ? (1 << 30) : (int) len;
        int n = (int) recv(s, p, chunk, 0);
        if (n <= 0) {
#ifndef _WIN32
            if (n < 0 && errno == EINTR)
                continue;
#endif
            return -1;
        }
        p += n;
        len -= (size_t) n;
    }
    return 0;
}

/** Several buffers sent as one (one system call when the socket takes it all): 0, or -1.
 * The parts are left as they were. At most 8 parts. */
static inline int dn_send_parts(dn_sock s, const void *const *bufs, const size_t *lens, int n)
{
    size_t left[8];
    const char *ptr[8];
    int i, first = 0;
    for (i = 0; i < n; i++) {
        ptr[i] = (const char *) bufs[i];
        left[i] = lens[i];
    }
    while (first < n) {
#ifdef _WIN32
        WSABUF wb[8];
        DWORD sent = 0;
        int k = 0;
        for (i = first; i < n; i++, k++) {
            wb[k].buf = (char *) ptr[i];
            wb[k].len = (ULONG) (left[i] > (1u << 30) ? (1u << 30) : left[i]);
        }
        if (WSASend(s, wb, (DWORD) k, &sent, 0, NULL, NULL) != 0)
            return -1;
        size_t done = (size_t) sent;
#else
        struct iovec iov[8];
        struct msghdr msg;
        int k = 0;
        ssize_t sent;
        for (i = first; i < n; i++, k++) {
            iov[k].iov_base = (void *) ptr[i];
            iov[k].iov_len = left[i];
        }
        memset(&msg, 0, sizeof msg);
        msg.msg_iov = iov;
        msg.msg_iovlen = (size_t) k;
        sent = sendmsg(s, &msg, MSG_NOSIGNAL);
        if (sent < 0 && errno == EINTR)
            continue;
        if (sent <= 0)
            return -1;
        size_t done = (size_t) sent;
#endif
        while (first < n && done >= left[first]) {       /* the parts sent whole */
            done -= left[first];
            first++;
        }
        if (first < n) {                                  /* the part sent in part */
            ptr[first] += done;
            left[first] -= done;
        }
    }
    return 0;
}

/** Several buffers filled, in order, exactly: 0, or -1. At most 8 parts. */
static inline int dn_recv_parts(dn_sock s, void *const *bufs, const size_t *lens, int n)
{
    size_t left[8];
    char *ptr[8];
    int i, first = 0;
    for (i = 0; i < n; i++) {
        ptr[i] = (char *) bufs[i];
        left[i] = lens[i];
    }
    while (first < n && left[first] == 0)
        first++;
    while (first < n) {
#ifdef _WIN32
        WSABUF wb[8];
        DWORD got = 0, flags = 0;
        int k = 0;
        for (i = first; i < n; i++, k++) {
            wb[k].buf = ptr[i];
            wb[k].len = (ULONG) (left[i] > (1u << 30) ? (1u << 30) : left[i]);
        }
        if (WSARecv(s, wb, (DWORD) k, &got, &flags, NULL, NULL) != 0 || got == 0)
            return -1;
        size_t done = (size_t) got;
#else
        struct iovec iov[8];
        struct msghdr msg;
        int k = 0;
        ssize_t got;
        for (i = first; i < n; i++, k++) {
            iov[k].iov_base = ptr[i];
            iov[k].iov_len = left[i];
        }
        memset(&msg, 0, sizeof msg);
        msg.msg_iov = iov;
        msg.msg_iovlen = (size_t) k;
        got = recvmsg(s, &msg, 0);
        if (got < 0 && errno == EINTR)
            continue;
        if (got <= 0)
            return -1;
        size_t done = (size_t) got;
#endif
        while (first < n && done >= left[first]) {
            done -= left[first];
            first++;
        }
        if (first < n) {
            ptr[first] += done;
            left[first] -= done;
        }
    }
    return 0;
}

/** A pause for spin loops: tells the CPU (and its sibling hyper-thread) we are waiting. */
static inline void dn_cpu_relax(void)
{
#if defined(_MSC_VER)
    YieldProcessor();
#elif defined(__x86_64__) || defined(__i386__)
    __builtin_ia32_pause();
#elif defined(__aarch64__)
    __asm__ __volatile__("yield");
#endif
}

/** Spin until s has data to read (no system call sleep): 1 ready, 0 after timeout_ms, -1 error. */
static inline int dn_spin_readable(dn_sock s, int timeout_ms)
{
    int64_t spins = 0;
    time_t t0 = time(NULL);
    for (;;) {
#ifdef _WIN32
        u_long n = 0;
        if (ioctlsocket(s, FIONREAD, &n) != 0)
            return -1;
        if (n > 0)
            return 1;
#else
        char c;
        ssize_t n = recv(s, &c, 1, MSG_PEEK | MSG_DONTWAIT);
        if (n > 0)
            return 1;
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
            return -1;                                   /* closed, or an error */
#endif
        dn_cpu_relax();
        if ((++spins & 0xFFFF) == 0 && (int) difftime(time(NULL), t0) * 1000 >= timeout_ms)
            return 0;
    }
}

/** One text line (without its '\n') into line, or -1. Lines are short: byte by byte. */
static inline int dn_recv_line(dn_sock s, char *line, size_t len)
{
    size_t i = 0;
    while (i + 1 < len) {
        char c;
        if (recv(s, &c, 1, 0) != 1)
            return -1;
        if (c == '\n')
            break;
        if (c != '\r')
            line[i++] = c;
    }
    line[i] = '\0';
    return 0;
}

static inline int dn_send_str(dn_sock s, const char *text)
{
    return dn_send_all(s, text, strlen(text));
}

/** A TCP connection to host:port within timeout_ms, tuned; DN_INVALID on failure. */
static inline dn_sock dn_connect(const char *host, int port, int timeout_ms)
{
    char sport[16];
    struct addrinfo hints, *res = NULL, *a;
    dn_sock s = DN_INVALID;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(sport, sizeof sport, "%d", port);
    if (getaddrinfo(host, sport, &hints, &res) != 0)
        return DN_INVALID;
    for (a = res; a != NULL; a = a->ai_next) {
        s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (s == DN_INVALID)
            continue;
        /* non-blocking connect, so a host that does not answer costs timeout_ms at most */
#ifdef _WIN32
        u_long nb = 1;
        ioctlsocket(s, FIONBIO, &nb);
#else
        int flags = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
        int rc = connect(s, a->ai_addr, (int) a->ai_addrlen);
        int ok = rc == 0;
        if (!ok) {
            fd_set w, e;
            struct timeval tv;
            FD_ZERO(&w);
            FD_ZERO(&e);
            FD_SET(s, &w);
            FD_SET(s, &e);
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            if (select((int) s + 1, NULL, &w, &e, &tv) > 0 && FD_ISSET(s, &w)) {
                int err = 0;
                socklen_t el = sizeof err;
                getsockopt(s, SOL_SOCKET, SO_ERROR, (char *) &err, &el);
                ok = err == 0;
            }
        }
#ifdef _WIN32
        nb = 0;
        ioctlsocket(s, FIONBIO, &nb);
#else
        fcntl(s, F_SETFL, flags);
#endif
        if (ok)
            break;
        dn_close(s);
        s = DN_INVALID;
    }
    freeaddrinfo(res);
    if (s != DN_INVALID)
        dn_tune(s);
    return s;
}

/** A listening TCP socket on bind_ip:port (NULL: every interface), or DN_INVALID. */
static inline dn_sock dn_listen(const char *bind_ip, int port)
{
    struct sockaddr_in addr;
    int one = 1;
    dn_sock s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == DN_INVALID)
        return DN_INVALID;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *) &one, sizeof one);
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short) port);
    addr.sin_addr.s_addr = bind_ip ? inet_addr(bind_ip) : htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *) &addr, sizeof addr) != 0 || listen(s, 64) != 0) {
        dn_close(s);
        return DN_INVALID;
    }
    return s;
}

/* ------------------------------------------------------------------ threads */

#ifdef _WIN32
typedef CRITICAL_SECTION   dn_mutex;
typedef CONDITION_VARIABLE dn_cond;
static inline void dn_mutex_init(dn_mutex *m) { InitializeCriticalSection(m); }
static inline void dn_mutex_destroy(dn_mutex *m) { DeleteCriticalSection(m); }
static inline void dn_lock(dn_mutex *m) { EnterCriticalSection(m); }
static inline void dn_unlock(dn_mutex *m) { LeaveCriticalSection(m); }
static inline void dn_cond_init(dn_cond *c) { InitializeConditionVariable(c); }
static inline void dn_cond_destroy(dn_cond *c) { (void) c; }
static inline void dn_cond_broadcast(dn_cond *c) { WakeAllConditionVariable(c); }
/** Wait on c (m locked) for at most ms. */
static inline void dn_cond_wait_ms(dn_cond *c, dn_mutex *m, int ms) { SleepConditionVariableCS(c, m, (DWORD) ms); }

typedef struct { void *(*fn)(void *); void *arg; } dn_thread_start_args;
static inline DWORD WINAPI dn_thread_trampoline(LPVOID p)
{
    dn_thread_start_args a = *(dn_thread_start_args *) p;
    free(p);
    a.fn(a.arg);
    return 0;
}
/** A detached thread running fn(arg); 0 or -1. */
static inline int dn_thread(void *(*fn)(void *), void *arg)
{
    dn_thread_start_args *a = (dn_thread_start_args *) malloc(sizeof *a);
    HANDLE h;
    if (!a)
        return -1;
    a->fn = fn;
    a->arg = arg;
    h = CreateThread(NULL, 0, dn_thread_trampoline, a, 0, NULL);
    if (!h) {
        free(a);
        return -1;
    }
    CloseHandle(h);
    return 0;
}
static inline void dn_sleep_ms(int ms) { Sleep((DWORD) ms); }
#else
typedef pthread_mutex_t dn_mutex;
typedef pthread_cond_t  dn_cond;
static inline void dn_mutex_init(dn_mutex *m) { pthread_mutex_init(m, NULL); }
static inline void dn_mutex_destroy(dn_mutex *m) { pthread_mutex_destroy(m); }
static inline void dn_lock(dn_mutex *m) { pthread_mutex_lock(m); }
static inline void dn_unlock(dn_mutex *m) { pthread_mutex_unlock(m); }
static inline void dn_cond_init(dn_cond *c) { pthread_cond_init(c, NULL); }
static inline void dn_cond_destroy(dn_cond *c) { pthread_cond_destroy(c); }
static inline void dn_cond_broadcast(dn_cond *c) { pthread_cond_broadcast(c); }
static inline void dn_cond_wait_ms(dn_cond *c, dn_mutex *m, int ms)
{
    struct timespec t;
    timespec_get(&t, TIME_UTC);                    /* pthread_cond_timedwait: CLOCK_REALTIME */
    t.tv_sec += ms / 1000;
    t.tv_nsec += (long) (ms % 1000) * 1000000L;
    if (t.tv_nsec >= 1000000000L) {
        t.tv_sec++;
        t.tv_nsec -= 1000000000L;
    }
    pthread_cond_timedwait(c, m, &t);
}
static inline int dn_thread(void *(*fn)(void *), void *arg)
{
    pthread_t t;
    if (pthread_create(&t, NULL, fn, arg) != 0)
        return -1;
    pthread_detach(t);
    return 0;
}
static inline void dn_sleep_ms(int ms)
{
    struct timespec t;
    t.tv_sec = ms / 1000;
    t.tv_nsec = (long) (ms % 1000) * 1000000L;
    nanosleep(&t, NULL);
}
#endif

/* ------------------------------------------------------------------- clocks */

/** Wall clock, UTC, ns (the SHMs' write times use it). */
static inline int64_t dn_real_ns(void)
{
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (int64_t) t.tv_sec * 1000000000LL + t.tv_nsec;
}

/** A monotonic clock, ns (intervals). */
static inline int64_t dn_mono_ns(void)
{
#ifdef _WIN32
    static LARGE_INTEGER f;
    LARGE_INTEGER c;
    if (f.QuadPart == 0)
        QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (int64_t) ((double) c.QuadPart * 1e9 / (double) f.QuadPart);
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t) t.tv_sec * 1000000000LL + t.tv_nsec;
#endif
}

/* -------------------------------------------------------------------- files */

/** Call fn(dir/entry, entry, arg) for each file of dir ending with suffix. */
static inline void dn_list_dir(const char *dir, const char *suffix,
                        void (*fn)(const char *path, const char *entry, void *arg), void *arg)
{
    char path[1024];
#ifdef _WIN32
    WIN32_FIND_DATAA fd;
    HANDLE h;
    snprintf(path, sizeof path, "%s\\*%s", dir, suffix);
    h = FindFirstFileA(path, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            snprintf(path, sizeof path, "%s\\%s", dir, fd.cFileName);
            fn(path, fd.cFileName, arg);
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    struct dirent *e;
    size_t sl = strlen(suffix);
    if (!d)
        return;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);
        if (n > sl && strcmp(e->d_name + n - sl, suffix) == 0) {
            snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
            fn(path, e->d_name, arg);
        }
    }
    closedir(d);
#endif
}

static inline int dn_file_exists(const char *path)
{
#ifdef _WIN32
    DWORD a = GetFileAttributesA(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

/** The file's identity and change time, to notice a new or replaced SHM. */
static inline uint64_t dn_file_stamp(const char *path)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &a))
        return 0;
    return ((uint64_t) a.ftCreationTime.dwHighDateTime << 32 | a.ftCreationTime.dwLowDateTime)
           ^ ((uint64_t) a.nFileSizeLow << 1);
#else
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;
    /* identity and size only: the times move with every write of a mapped SHM */
    return (uint64_t) st.st_ino * 1000003u ^ (uint64_t) st.st_size ^ ((uint64_t) st.st_dev << 40);
#endif
}

static inline void dn_remove(const char *path)
{
#ifdef _WIN32
    DeleteFileA(path);
#else
    unlink(path);
#endif
}

/** This machine's name, without its domain. */
static inline void dn_hostname(char *name, size_t len)
{
    char *dot;
    if (gethostname(name, (int) len) != 0)
        snprintf(name, len, "localhost");
    name[len - 1] = '\0';
    dot = strchr(name, '.');
    if (dot)
        *dot = '\0';
}

#endif /* DAO_NET_PLATFORM_H */
