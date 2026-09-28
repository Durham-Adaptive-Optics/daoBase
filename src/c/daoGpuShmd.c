/**
 * @file    daoGpuShmd.c
 * @brief   Keeps the GPU payloads of dao GPU SHMs alive.
 *
 * libdao hands this daemon the POSIX descriptor of each GPU SHM allocation
 * (cuMemExportToShareableHandle). While the daemon holds it, the allocation
 * survives its creator exiting or crashing, and new processes get the
 * descriptor from here to map the payload. An entry is dropped, and the GPU
 * memory freed once no process maps it any more, when its /tmp SHM file is
 * removed or replaced. No CUDA call is made here.
 *
 * Usage: daoGpuShmd            serve (libdao starts it on demand)
 *        daoGpuShmd --list     list the payloads held by the running daemon
 *        daoGpuShmd --ping     exit 0 if a daemon is running
 *
 * Socket: /tmp/daoGpuShmd-<uid>.sock, or $DAO_GPU_SOCKET.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include "daoGpuProto.h"

typedef struct {
    char     name[256];
    uint64_t id;
    uint64_t size;
    uint64_t ino;
    int32_t  pid;
    int      fd;
} Entry;

static Entry *entries = NULL;
static size_t n_entries = 0, cap_entries = 0;
static char socket_path[108];
static volatile sig_atomic_t stop_requested = 0;

static void logmsg(const char *fmt, ...)
{
    char stamp[32];
    time_t now = time(NULL);
    va_list ap;
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "%s daoGpuShmd: ", stamp);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

static Entry *find(const char *name)
{
    for (size_t i = 0; i < n_entries; i++)
        if (strcmp(entries[i].name, name) == 0)
            return &entries[i];
    return NULL;
}

static void drop_at(size_t i, const char *why)
{
    logmsg("drop %s (id %llx, %llu B): %s", entries[i].name,
           (unsigned long long) entries[i].id, (unsigned long long) entries[i].size, why);
    close(entries[i].fd);
    entries[i] = entries[--n_entries];
}

static void put(const daoGpuMsg *msg, int fd)
{
    Entry *e = find(msg->name);
    if (e) {
        logmsg("replace %s (id %llx -> %llx)", e->name,
               (unsigned long long) e->id, (unsigned long long) msg->id);
        close(e->fd);
    } else {
        if (n_entries == cap_entries) {
            cap_entries = cap_entries ? 2 * cap_entries : 16;
            entries = realloc(entries, cap_entries * sizeof *entries);
        }
        e = &entries[n_entries++];
        snprintf(e->name, sizeof e->name, "%s", msg->name);
    }
    e->id = msg->id;
    e->size = msg->size;
    e->ino = msg->ino;
    e->pid = msg->pid;
    e->fd = fd;
    logmsg("hold %s (id %llx, %llu B, from pid %d)", e->name,
           (unsigned long long) e->id, (unsigned long long) e->size, (int) e->pid);
}

/* Drop entries whose SHM file is gone or was recreated as another file. */
static void housekeeping(void)
{
    struct stat st;
    for (size_t i = 0; i < n_entries;) {
        if (stat(entries[i].name, &st) != 0)
            drop_at(i, "SHM file removed");
        else if ((uint64_t) st.st_ino != entries[i].ino)
            drop_at(i, "SHM file replaced");
        else
            i++;
    }
}

static void serve_client(int c)
{
    daoGpuMsg req, rep;
    int fd;
    if (daoGpuRecv(c, &req, &fd) != 0) {
        if (fd >= 0)
            close(fd);
        return;
    }
    req.name[sizeof req.name - 1] = '\0';
    daoGpuMsgInit(&rep, DAO_GPU_OP_OK, req.name);

    switch (req.op) {
    case DAO_GPU_OP_PUT:
        if (fd < 0) {
            rep.op = DAO_GPU_OP_ERR;
            snprintf(rep.text, sizeof rep.text, "PUT without a descriptor");
        } else {
            put(&req, fd);
            rep.id = req.id;
        }
        daoGpuSend(c, &rep, -1);
        break;
    case DAO_GPU_OP_GET: {
        Entry *e = find(req.name);
        if (!e) {
            rep.op = DAO_GPU_OP_ERR;
            snprintf(rep.text, sizeof rep.text, "no GPU payload held for this SHM");
            daoGpuSend(c, &rep, -1);
        } else {
            rep.id = e->id;
            rep.size = e->size;
            daoGpuSend(c, &rep, e->fd);
        }
        break;
    }
    case DAO_GPU_OP_DROP:
        for (size_t i = 0; i < n_entries; i++)
            if (strcmp(entries[i].name, req.name) == 0 && (!req.id || entries[i].id == req.id)) {
                drop_at(i, "dropped on request");
                break;
            }
        daoGpuSend(c, &rep, -1);
        break;
    case DAO_GPU_OP_LIST:
        for (size_t i = 0; i < n_entries; i++) {
            daoGpuMsg item;
            daoGpuMsgInit(&item, DAO_GPU_OP_OK, entries[i].name);
            item.id = entries[i].id;
            item.size = entries[i].size;
            item.pid = entries[i].pid;
            daoGpuSend(c, &item, -1);
        }
        rep.op = DAO_GPU_OP_END;
        daoGpuSend(c, &rep, -1);
        break;
    case DAO_GPU_OP_PING:
        rep.pid = (int32_t) getpid();
        daoGpuSend(c, &rep, -1);
        break;
    default:
        rep.op = DAO_GPU_OP_ERR;
        snprintf(rep.text, sizeof rep.text, "unknown request '%c'", req.op);
        daoGpuSend(c, &rep, -1);
        if (fd >= 0)
            close(fd);
    }
}

static void on_signal(int sig)
{
    (void) sig;
    stop_requested = 1;
}

static int client_command(char op)
{
    daoGpuMsg req, rep;
    int s = daoGpuConnect(), fd;
    if (s < 0) {
        fprintf(stderr, "daoGpuShmd is not running\n");
        return 1;
    }
    daoGpuMsgInit(&req, op, NULL);
    daoGpuSend(s, &req, -1);
    if (op == DAO_GPU_OP_PING) {
        int ok = daoGpuRecv(s, &rep, &fd) == 0 && rep.op == DAO_GPU_OP_OK;
        if (ok)
            printf("daoGpuShmd running, pid %d\n", (int) rep.pid);
        close(s);
        return ok ? 0 : 1;
    }
    printf("%-18s %12s %8s  %s\n", "id", "bytes", "creator", "SHM");
    while (daoGpuRecv(s, &rep, &fd) == 0 && rep.op != DAO_GPU_OP_END)
        printf("%016llx %12llu %8d  %s\n", (unsigned long long) rep.id,
               (unsigned long long) rep.size, (int) rep.pid, rep.name);
    close(s);
    return 0;
}

int main(int argc, char **argv)
{
    struct sockaddr_un a;
    int l, probe;

    if (argc > 1 && strcmp(argv[1], "--list") == 0)
        return client_command(DAO_GPU_OP_LIST);
    if (argc > 1 && strcmp(argv[1], "--ping") == 0)
        return client_command(DAO_GPU_OP_PING);
    if (argc > 1) {
        fprintf(stderr, "usage: daoGpuShmd [--list | --ping]\n");
        return 2;
    }

    daoGpuSocketPath(socket_path, sizeof socket_path);
    probe = daoGpuConnect();
    if (probe >= 0) {                      /* one daemon per socket */
        close(probe);
        logmsg("already running on %s", socket_path);
        return 0;
    }
    unlink(socket_path);                   /* stale socket from a daemon that died */

    l = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    snprintf(a.sun_path, sizeof a.sun_path, "%s", socket_path);
    {
        mode_t old = umask(0077);          /* owner only, like the SHM files */
        int rc = bind(l, (struct sockaddr *) &a, sizeof a);
        umask(old);
        if (rc != 0 || listen(l, 64) != 0) {
            logmsg("cannot listen on %s: %s", socket_path, strerror(errno));
            return 1;
        }
    }
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);
    logmsg("listening on %s (pid %d)", socket_path, (int) getpid());

    time_t last_check = 0;
    while (!stop_requested) {
        struct pollfd p = { l, POLLIN, 0 };
        int r = poll(&p, 1, 1000);
        if (r > 0 && (p.revents & POLLIN)) {
            int c = accept4(l, NULL, NULL, SOCK_CLOEXEC);
            if (c >= 0) {
                struct timeval tv = { 2, 0 };   /* a stuck client cannot block the daemon */
                setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
                setsockopt(c, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
                serve_client(c);
                close(c);
            }
        }
        if (time(NULL) != last_check) {
            housekeeping();
            last_check = time(NULL);
        }
    }
    logmsg("stopping, releasing %zu payload(s)", n_entries);
    while (n_entries)
        drop_at(n_entries - 1, "daemon stopping");
    close(l);
    unlink(socket_path);
    return 0;
}
