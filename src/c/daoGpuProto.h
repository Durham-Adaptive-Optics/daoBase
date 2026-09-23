/**
 * @file    daoGpuProto.h
 * @brief   Protocol between libdao (daoGpu.c) and the daoGpuShmd daemon.
 *
 * daoGpuShmd keeps, for each GPU SHM, the POSIX file descriptor exported by
 * cuMemExportToShareableHandle. Holding that descriptor keeps the GPU
 * allocation alive after the creating process exits or crashes, and lets new
 * processes import it. The daemon never calls CUDA.
 *
 * One daemon per user, on a Unix socket (default /tmp/daoGpuShmd-<uid>.sock,
 * overridden by $DAO_GPU_SOCKET). Every request and reply is one daoGpuMsg;
 * a descriptor travels as SCM_RIGHTS ancillary data. Internal header, not
 * installed.
 */
#ifndef DAO_GPU_PROTO_H
#define DAO_GPU_PROTO_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define DAO_GPU_PROTO_MAGIC 0x44474d31u   /* 'DGM1' */

enum {
    DAO_GPU_OP_PUT  = 'P',   /* register name -> fd (fd attached); replaces an older entry */
    DAO_GPU_OP_GET  = 'G',   /* reply carries the fd                                        */
    DAO_GPU_OP_DROP = 'D',   /* forget name (the allocation is freed once nobody maps it)  */
    DAO_GPU_OP_LIST = 'L',   /* one reply per entry, then DAO_GPU_OP_END                    */
    DAO_GPU_OP_PING = 'Q',
    DAO_GPU_OP_OK   = 'K',
    DAO_GPU_OP_ERR  = 'E',
    DAO_GPU_OP_END  = 'Z',
};

typedef struct {
    uint32_t magic;
    char     op;
    char     name[256];      /* SHM file path, e.g. /tmp/wfs.im.shm */
    uint64_t id;             /* md[0].gpu_id of the allocation       */
    uint64_t size;           /* bytes                                */
    uint64_t ino;            /* inode of the SHM file at PUT time    */
    int32_t  pid;            /* PUT: creator pid; LIST: -            */
    char     text[128];      /* ERR: reason                          */
} daoGpuMsg;

static inline void daoGpuSocketPath(char *out, size_t len)
{
    const char *env = getenv("DAO_GPU_SOCKET");
    if (env && *env)
        snprintf(out, len, "%s", env);
    else
        snprintf(out, len, "/tmp/daoGpuShmd-%u.sock", (unsigned) getuid());
}

/* Send one message, with an optional descriptor (fd < 0: none). */
static inline int daoGpuSend(int sock, const daoGpuMsg *msg, int fd)
{
    struct msghdr m;
    struct iovec io;
    char ctrl[CMSG_SPACE(sizeof(int))];
    memset(&m, 0, sizeof m);
    io.iov_base = (void *) msg;
    io.iov_len = sizeof *msg;
    m.msg_iov = &io;
    m.msg_iovlen = 1;
    if (fd >= 0) {
        struct cmsghdr *c;
        memset(ctrl, 0, sizeof ctrl);
        m.msg_control = ctrl;
        m.msg_controllen = sizeof ctrl;
        c = CMSG_FIRSTHDR(&m);
        c->cmsg_level = SOL_SOCKET;
        c->cmsg_type = SCM_RIGHTS;
        c->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(c), &fd, sizeof fd);
    }
    return sendmsg(sock, &m, MSG_NOSIGNAL) == (ssize_t) sizeof *msg ? 0 : -1;
}

/* Receive one message; *fd is the attached descriptor or -1. */
static inline int daoGpuRecv(int sock, daoGpuMsg *msg, int *fd)
{
    struct msghdr m;
    struct iovec io;
    char ctrl[CMSG_SPACE(sizeof(int))];
    struct cmsghdr *c;
    ssize_t n;
    memset(&m, 0, sizeof m);
    io.iov_base = msg;
    io.iov_len = sizeof *msg;
    m.msg_iov = &io;
    m.msg_iovlen = 1;
    m.msg_control = ctrl;
    m.msg_controllen = sizeof ctrl;
    *fd = -1;
    n = recvmsg(sock, &m, MSG_WAITALL | MSG_CMSG_CLOEXEC);
    if (n != (ssize_t) sizeof *msg || msg->magic != DAO_GPU_PROTO_MAGIC)
        return -1;
    for (c = CMSG_FIRSTHDR(&m); c; c = CMSG_NXTHDR(&m, c))
        if (c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS)
            memcpy(fd, CMSG_DATA(c), sizeof(int));
    return 0;
}

static inline int daoGpuConnect(void)
{
    struct sockaddr_un a;
    int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (s < 0)
        return -1;
    memset(&a, 0, sizeof a);
    a.sun_family = AF_UNIX;
    daoGpuSocketPath(a.sun_path, sizeof a.sun_path);
    if (connect(s, (struct sockaddr *) &a, sizeof a) != 0) {
        close(s);
        return -1;
    }
    return s;
}

static inline void daoGpuMsgInit(daoGpuMsg *msg, char op, const char *name)
{
    memset(msg, 0, sizeof *msg);
    msg->magic = DAO_GPU_PROTO_MAGIC;
    msg->op = op;
    if (name)
        snprintf(msg->name, sizeof msg->name, "%s", name);
}

#endif /* DAO_GPU_PROTO_H */
