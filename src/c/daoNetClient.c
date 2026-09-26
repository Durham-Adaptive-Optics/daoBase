/**
 * @file    daoNetClient.c
 * @brief   daoNetResolve: libdao's side of the network SHMs (daoNet.h).
 *
 * When a process opens an SHM that is not on its machine, daoShmOpen (and the
 * language bindings) ask the local daoShmNetd for it here: one text line on its
 * control port (127.0.0.1 only), answered with the path of the local replica once
 * it holds the SHM's data. Without a service the connection is refused at once, so
 * an open of a missing SHM fails as fast as before.
 */
#include "daoNetPlatform.h"          /* first: feature macros, winsock2.h before windows.h */
#include "daoNet.h"

static int env_int(const char *var, int fallback)
{
    const char *v = getenv(var);
    if (v && *v) {
        char *end;
        long x = strtol(v, &end, 10);
        if (*end == '\0' && x > 0 && x < 1000000000L)
            return (int) x;
    }
    return fallback;
}

DAO_NET_EXPORT int daoNetResolve(const char *name, char *path, size_t len, int timeout_ms)
{
    const char *enabled = getenv(DAO_NET_ENV_ENABLE);
    char request[1024], reply[1024];
    dn_sock s;
    int ok = 0;

    if (!name || !*name || !path || len == 0 || (enabled && strcmp(enabled, "0") == 0))
        return 1;
    if (strchr(name, '\n') || strchr(name, ' ') || strlen(name) > 900)
        return 1;
    if (timeout_ms <= 0)
        timeout_ms = env_int(DAO_NET_ENV_TIMEOUT, DAO_NET_RESOLVE_MS);
    if (dn_net_init() != 0)
        return 1;

    s = dn_connect("127.0.0.1", env_int(DAO_NET_ENV_CONTROL, DAO_NET_CONTROL_PORT), 200);
    if (s == DN_INVALID)
        return 1;                                   /* no service: the open fails as before */
    dn_recv_timeout(s, timeout_ms + 2000);
    snprintf(request, sizeof request, "WANT %s %d\n", name, timeout_ms);
    if (dn_send_str(s, request) == 0 && dn_recv_line(s, reply, sizeof reply) == 0
        && strncmp(reply, "OK ", 3) == 0 && strlen(reply + 3) < len) {
        strcpy(path, reply + 3);
        ok = dn_file_exists(path);
    }
    dn_close(s);
    return ok ? 0 : 1;
}
