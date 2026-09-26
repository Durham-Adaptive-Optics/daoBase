/**
 * @file    daoShmNetd.c
 * @brief   The network SHM service: every dao SHM of the network, opened by its name.
 *
 * One daoShmNetd per machine. The services find each other (UDP multicast within a
 * domain, or listed peers) and exchange the lists of their SHMs -- no data yet.
 * When a local process opens an SHM this machine does not have, libdao asks this
 * service (daoNetResolve, on the control port, 127.0.0.1 only): it subscribes to
 * the machine owning it and keeps a local replica with the same name up to date.
 *
 *   owner                                          reader
 *   writer -> /tmp/X.im.shm                        /tmp/X.im.shm -> any process, unchanged
 *                 | semaphore DAO_NET_SEM                 ^ written in place, published
 *                 v (no polling)                          | (counter + 1, semaphores posted)
 *            Source: one watcher  --- TCP: header + frame + trailer ---> Replica
 *            Session per reader   <-- WRITE: a local write on the replica --
 *
 * - the owner copies each frame once per reader, consistently (the SHM counter
 *   and write flag checked around the copy: never a torn frame), and always sends
 *   the newest: a reader slower than the writer skips frames (counted), its
 *   latency never grows;
 * - the reader receives straight into its replica, and publishes it as a write;
 * - a write made on a replica is sent to the owner, which writes the SHM: every
 *   replica gets it (the writer's own marked as an echo, not published twice);
 * - nothing is sent for an SHM nobody reads; a replica no process maps any more is
 *   dropped (Linux; elsewhere daoShmNet release);
 * - the replica carries its freshness: keywords NET_REPLICA (the owner),
 *   NET_SRC_CNT, NET_SRC_TIME, NET_AGE_US, NET_DROPPED, NET_LINK.
 *
 * Usage: daoShmNetd [options]          (daoShmNet.py start runs it in the background)
 *   --name NODE          this machine's name on the network (default: its hostname)
 *   --domain NAME        only services of this domain see each other (default: dao)
 *   --dir DIR            where the SHMs are (default: /tmp; Windows: DAO_SHM_DIR or .)
 *   --replica-prefix P   replicas named P<name> (a second service on one machine)
 *   --port N             data port, TCP (default 7710)
 *   --control-port N     control port, TCP on 127.0.0.1 (default 7709)
 *   --beacon-port N      discovery port, UDP (default 7711)
 *   --group ADDR         discovery multicast group (default 239.255.77.10)
 *   --iface ADDR         the network interface's address, for the multicast
 *   --bind ADDR          listen on that address only (default: all)
 *   --peer HOST[:PORT]   a service to reach without multicast (repeatable)
 *   --allow PREFIX       accept only addresses starting so (repeatable; default all)
 *   --no-multicast       no discovery beacons (listed peers only)
 *   --idle SECONDS       drop a replica unmapped for so long (default 30; 0: never)
 *   --read-only          refuse writes coming from other machines
 *   --spin               low latency: the streams poll instead of sleeping (a CPU core
 *                        busy per active stream, on each side; idle when none)
 *   -v                   verbose
 */
#include "daoNetPlatform.h"          /* first: on Windows, winsock2.h before windows.h */
#include "dao.h"
#include "daoNet.h"

#include <signal.h>
#include <stddef.h>
#include <stdarg.h>

#define NETD_VERSION     "1.0"
#define MAX_PEERS        64
#define MAX_LIST         16
#define NET_KW_COUNT     6
#define CATALOG_TICK_MS  1000
#define PEER_EXPIRE_NS   (6LL * 1000000000LL)
#define LINK_TIMEOUT_MS  4000
#define HEARTBEAT_NS     (1LL * 1000000000LL)

#ifdef _MSC_VER
#  define MEMORY_BARRIER() MemoryBarrier()
#else
#  define MEMORY_BARRIER() __sync_synchronize()
#endif

_Static_assert(sizeof(DaoNetHeader) == 64, "DaoNetHeader: 64 bytes on the wire");
_Static_assert(sizeof(DaoNetMeta) == 288, "DaoNetMeta: 288 bytes on the wire");
_Static_assert(sizeof(DaoNetKeyword) == 120, "DaoNetKeyword: 120 bytes on the wire");

/* ========================================================================== */
/* Configuration and logging                                                  */
/* ========================================================================== */

typedef struct {
    char node[64], domain[64], dir[512], prefix[64];
    char bind_ip[64], iface[64], group[64];
    int  data_port, control_port, beacon_port;
    int  multicast, idle_s, read_only, verbose, spin;
    char peers[MAX_LIST][128];
    int  n_peers;
    char allow[MAX_LIST][64];
    int  n_allow;
} Config;

static Config cfg;
static volatile int stopping = 0;

static void logmsg(int verbose_only, const char *fmt, ...)
{
    char stamp[32];
    time_t now;
    va_list ap;
    if (verbose_only && !cfg.verbose)
        return;
    now = time(NULL);
    strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(stderr, "%s daoShmNetd[%s]: ", stamp, cfg.node);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

static int address_allowed(const char *ip)
{
    int i;
    if (cfg.n_allow == 0 || strcmp(ip, "127.0.0.1") == 0)
        return 1;
    for (i = 0; i < cfg.n_allow; i++)
        if (strncmp(ip, cfg.allow[i], strlen(cfg.allow[i])) == 0)
            return 1;
    return 0;
}

static size_t atype_size(int atype)
{
    static const size_t sizes[] = {0, 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 8, 16};
    return atype > 0 && atype <= 12 ? sizes[atype] : 0;
}

/* "rtc1:/tmp/dm1Cmd.im.shm", "dm1Cmd", ... -> node ("" if none) and bare name */
static void split_spec(const char *spec, char *node, size_t nlen, char *name, size_t len)
{
    const char *colon = strchr(spec, ':');
    const char *base, *p;
    size_t n;
    node[0] = '\0';
    /* a node prefix: at least 2 characters, no path separator (C:\ is a drive) */
    if (colon && colon - spec >= 2 && !memchr(spec, '/', (size_t) (colon - spec))
        && !memchr(spec, '\\', (size_t) (colon - spec))) {
        n = (size_t) (colon - spec) < nlen - 1 ? (size_t) (colon - spec) : nlen - 1;
        memcpy(node, spec, n);
        node[n] = '\0';
        spec = colon + 1;
    }
    base = spec;
    for (p = spec; *p; p++)
        if (*p == '/' || *p == '\\')
            base = p + 1;
    snprintf(name, len, "%s", base);
    n = strlen(name);
    if (n > 7 && strcmp(name + n - 7, ".im.shm") == 0)
        name[n - 7] = '\0';
}

static void shm_path(char *path, size_t len, const char *prefix, const char *name)
{
    snprintf(path, len, "%s%c%s%s.im.shm", cfg.dir, DN_PATHSEP, prefix, name);
}

/* ========================================================================== */
/* The local SHMs (read from their headers: no SHM is opened to list it)      */
/* ========================================================================== */

typedef struct {
    char     name[256];
    uint8_t  naxis, atype;
    uint16_t nbkw;
    uint32_t size[3], fifo;
    int      gpu;
    uint64_t stamp;
    int      seen;
} ShmInfo;

static dn_mutex catalog_lock;
static ShmInfo *catalog = NULL;
static int n_catalog = 0, cap_catalog = 0;
static uint64_t catalog_version = 1;

/** What changes when an SHM is (re)created, even in place with the same size: the
 * file's identity and size, and its header's creation time and shape (the start of
 * the header only, a few hundred bytes). */
static uint64_t shm_stamp(const char *path)
{
    IMAGE_METADATA md;
    size_t head = offsetof(IMAGE_METADATA, last_access);
    uint64_t stamp = dn_file_stamp(path), h = 1469598103934665603ULL;
    const unsigned char *p = (const unsigned char *) &md;
    size_t i;
    FILE *f = fopen(path, "rb");
    if (!f)
        return stamp;
    memset(&md, 0, head);
    if (fread(&md, head, 1, f) == 1)
        for (i = 0; i < head; i++)                      /* FNV-1a of the header's start */
            h = (h ^ p[i]) * 1099511628211ULL;
    fclose(f);
    return stamp ^ h;
}

/** The header of an SHM file: 0 and info filled, or -1 (not a dao SHM, a replica). */
static int read_shm_header(const char *path, ShmInfo *info)
{
    IMAGE_METADATA md;
    IMAGE_KEYWORD kw;
    FILE *f = fopen(path, "rb");
    size_t el;
    uint32_t i, fifo;
    long long offset;
    int replica = 0;
    if (!f)
        return -1;
    if (fread(&md, sizeof md, 1, f) != 1 || md.magic != DAO_SHM_MAGIC
        || md.layout != DAO_SHM_LAYOUT_VERSION || md.naxis < 1 || md.naxis > 3) {
        fclose(f);
        return -1;
    }
    el = atype_size(md.atype);
    fifo = md.fifo_size ? md.fifo_size : 1;
    offset = (long long) fifo * (long long) sizeof(IMAGE_METADATA)
             + (long long) fifo * (long long) md.nelement * (long long) el;
    /* what daoShmOpen would refuse (by exiting): an empty axis, a truncated file */
    for (i = 0; i < md.naxis; i++)
        if (md.size[i] < 1) {
            fclose(f);
            return -1;
        }
    if (fseek(f, 0, SEEK_END) != 0 || ftell(f) < (long) offset) {
        fclose(f);
        return -1;
    }
    /* a replica (its keyword NET_REPLICA) is not ours to offer */
    if (md.NBkw > 0 && fseek(f, (long) offset, SEEK_SET) == 0)
        for (i = 0; i < md.NBkw && fread(&kw, sizeof kw, 1, f) == 1; i++)
            if (strncmp(kw.name, "NET_REPLICA", sizeof kw.name) == 0)
                replica = 1;
    fclose(f);
    if (replica || el == 0)
        return -1;
    memset(info, 0, sizeof *info);
    info->naxis = md.naxis;
    info->atype = md.atype;
    info->nbkw = md.NBkw;
    for (i = 0; i < 3; i++)
        info->size[i] = i < md.naxis ? md.size[i] : 1;
    info->fifo = fifo;
    info->gpu = md.gpu_magic != 0;
    return 0;
}

static int is_our_replica(const char *path);

static void scan_entry(const char *path, const char *entry, void *arg)
{
    char name[256];
    size_t n = strlen(entry) - 7;                     /* without ".im.shm" */
    uint64_t stamp = shm_stamp(path);
    ShmInfo info;
    int i;
    (void) arg;
    if (n == 0 || n >= sizeof name)
        return;
    memcpy(name, entry, n);
    name[n] = '\0';
    if ((cfg.prefix[0] && strncmp(name, cfg.prefix, strlen(cfg.prefix)) == 0) || is_our_replica(path))
        return;                                        /* one of our replicas */
    for (i = 0; i < n_catalog; i++)
        if (strcmp(catalog[i].name, name) == 0) {
            if (catalog[i].stamp == stamp) {
                catalog[i].seen = 1;
                return;
            }
            break;
        }
    if (read_shm_header(path, &info) != 0) {
        if (i < n_catalog)
            catalog[i].seen = 0;                       /* now a replica, or unreadable: dropped */
        return;
    }
    snprintf(info.name, sizeof info.name, "%s", name);
    info.stamp = stamp;
    info.seen = 1;
    if (i == n_catalog) {
        if (n_catalog == cap_catalog) {
            int cap = cap_catalog ? 2 * cap_catalog : 64;
            ShmInfo *grown = (ShmInfo *) realloc(catalog, (size_t) cap * sizeof *grown);
            if (!grown)
                return;
            catalog = grown;
            cap_catalog = cap;
        }
        n_catalog++;
    }
    catalog[i] = info;
    catalog_version++;
}

static void scan_local(void)
{
    int i, j;
    dn_lock(&catalog_lock);
    for (i = 0; i < n_catalog; i++)
        catalog[i].seen = 0;
    dn_list_dir(cfg.dir, ".im.shm", scan_entry, NULL);
    for (i = 0, j = 0; i < n_catalog; i++)
        if (catalog[i].seen)
            catalog[j++] = catalog[i];
        else
            catalog_version++;
    n_catalog = j;
    dn_unlock(&catalog_lock);
}

static int local_has(const char *name, ShmInfo *out)
{
    int i, found = 0;
    dn_lock(&catalog_lock);
    for (i = 0; i < n_catalog && !found; i++)
        if (strcmp(catalog[i].name, name) == 0) {
            if (out)
                *out = catalog[i];
            found = 1;
        }
    dn_unlock(&catalog_lock);
    return found;
}

/* ========================================================================== */
/* The other services                                                          */
/* ========================================================================== */

typedef struct {
    char     node[64], addr[64];
    int      port, is_static, used;
    uint64_t version;
    int64_t  last_seen;
    ShmInfo *shms;
    int      n_shms;
} Peer;

static dn_mutex peers_lock;
static Peer peers[MAX_PEERS];

/** A peer's list, fetched from its data port: 0, or -1. Fills node too. */
static int fetch_catalog(const char *addr, int port, char *node, size_t nlen,
                         uint64_t *version, ShmInfo **shms, int *n_shms)
{
    char line[512];
    int n = 0, cap = 0;
    ShmInfo *list = NULL;
    dn_sock s = dn_connect(addr, port, 500);
    if (s == DN_INVALID)
        return -1;
    dn_recv_timeout(s, 2000);
    if (dn_send_str(s, "CATALOG\n") != 0 || dn_recv_line(s, line, sizeof line) != 0
        || strncmp(line, "NODE ", 5) != 0) {
        dn_close(s);
        return -1;
    }
    {
        char dom[64] = "";
        unsigned long long v = 0;
        char nd[64] = "";
        if (sscanf(line + 5, "%63s %llu %63s", nd, &v, dom) < 2 || (dom[0] && strcmp(dom, cfg.domain) != 0)) {
            dn_close(s);
            return -1;
        }
        snprintf(node, nlen, "%s", nd);
        *version = v;
    }
    while (dn_recv_line(s, line, sizeof line) == 0 && strcmp(line, "END") != 0) {
        ShmInfo in;
        unsigned naxis, atype, nbkw, fifo, gpu, s0, s1, s2;
        memset(&in, 0, sizeof in);
        if (sscanf(line, "SHM %255s %u %u %u %u %u %u %u %u", in.name, &naxis, &s0, &s1, &s2,
                   &atype, &nbkw, &fifo, &gpu) != 9)
            continue;
        in.naxis = (uint8_t) naxis;
        in.size[0] = s0;
        in.size[1] = s1;
        in.size[2] = s2;
        in.atype = (uint8_t) atype;
        in.nbkw = (uint16_t) nbkw;
        in.fifo = fifo;
        in.gpu = (int) gpu;
        if (n == cap) {
            ShmInfo *grown;
            cap = cap ? 2 * cap : 64;
            grown = (ShmInfo *) realloc(list, (size_t) cap * sizeof *grown);
            if (!grown)
                break;
            list = grown;
        }
        list[n++] = in;
    }
    dn_close(s);
    *shms = list;
    *n_shms = n;
    return 0;
}

/** Record a peer heard of (a beacon, or a listed peer's list): refresh its list if it changed. */
static void peer_seen(const char *node, const char *addr, int port, uint64_t version, int is_static)
{
    int i, slot = -1, refresh = 0;
    int64_t now = dn_mono_ns();
    dn_lock(&peers_lock);
    for (i = 0; i < MAX_PEERS; i++) {
        if (peers[i].used && strcmp(peers[i].node, node) == 0 && peers[i].port == port) {
            slot = i;
            break;
        }
        if (!peers[i].used && slot < 0)
            slot = i;
    }
    if (slot >= 0) {
        Peer *p = &peers[slot];
        if (!p->used) {
            memset(p, 0, sizeof *p);
            p->used = 1;
            snprintf(p->node, sizeof p->node, "%s", node);
            logmsg(0, "peer %s at %s:%d", node, addr, port);
        }
        snprintf(p->addr, sizeof p->addr, "%s", addr);
        p->port = port;
        p->is_static |= is_static;
        p->last_seen = now;
        refresh = p->version != version;
    }
    dn_unlock(&peers_lock);
    if (refresh) {
        char nd[64];
        uint64_t v;
        ShmInfo *shms = NULL;
        int n = 0;
        if (fetch_catalog(addr, port, nd, sizeof nd, &v, &shms, &n) == 0) {
            dn_lock(&peers_lock);
            if (peers[slot].used && strcmp(peers[slot].node, node) == 0) {
                free(peers[slot].shms);
                peers[slot].shms = shms;
                peers[slot].n_shms = n;
                peers[slot].version = v;
                shms = NULL;
            }
            dn_unlock(&peers_lock);
            logmsg(1, "peer %s: %d SHMs", node, n);
        }
        free(shms);
    }
}

static void expire_peers(void)
{
    int i;
    int64_t now = dn_mono_ns();
    dn_lock(&peers_lock);
    for (i = 0; i < MAX_PEERS; i++)
        if (peers[i].used && now - peers[i].last_seen > PEER_EXPIRE_NS) {
            logmsg(0, "peer %s gone", peers[i].node);
            free(peers[i].shms);
            memset(&peers[i], 0, sizeof peers[i]);
        }
    dn_unlock(&peers_lock);
}

/** Where `name` (optionally on `node`) is: 0 and addr/port/node filled, 1 ambiguous, -1 nowhere. */
static int find_remote(const char *node, const char *name, char *addr, size_t alen, int *port,
                       char *owner, size_t olen, ShmInfo *info)
{
    int i, j, found = 0;
    dn_lock(&peers_lock);
    for (i = 0; i < MAX_PEERS; i++) {
        if (!peers[i].used || (node[0] && strcmp(peers[i].node, node) != 0))
            continue;
        for (j = 0; j < peers[i].n_shms; j++)
            if (strcmp(peers[i].shms[j].name, name) == 0) {
                if (found++ == 0) {
                    snprintf(addr, alen, "%s", peers[i].addr);
                    snprintf(owner, olen, "%.63s", peers[i].node);
                    *port = peers[i].port;
                    if (info)
                        *info = peers[i].shms[j];
                }
                break;
            }
    }
    dn_unlock(&peers_lock);
    return found == 0 ? -1 : found > 1 ? 1 : 0;
}

/* ------------------------------------------------------------ discovery */

static dn_sock beacon_socket(void)
{
    struct sockaddr_in addr;
    struct ip_mreq mreq;
    int one = 1;
    unsigned char ttl = 1, loop = 1;
    dn_sock s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == DN_INVALID)
        return DN_INVALID;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *) &one, sizeof one);
#ifdef SO_REUSEPORT
    setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (const char *) &one, sizeof one);   /* several services, one machine */
#endif
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short) cfg.beacon_port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s, (struct sockaddr *) &addr, sizeof addr) != 0) {
        dn_close(s);
        return DN_INVALID;
    }
    if (cfg.multicast) {
        memset(&mreq, 0, sizeof mreq);
        mreq.imr_multiaddr.s_addr = inet_addr(cfg.group);
        mreq.imr_interface.s_addr = cfg.iface[0] ? inet_addr(cfg.iface) : htonl(INADDR_ANY);
        if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char *) &mreq, sizeof mreq) != 0)
            logmsg(0, "cannot join the multicast group %s: listed peers only", cfg.group);
        setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, (const char *) &ttl, sizeof ttl);
        setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (const char *) &loop, sizeof loop);
        if (cfg.iface[0]) {
            struct in_addr ifa;
            ifa.s_addr = inet_addr(cfg.iface);
            setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, (const char *) &ifa, sizeof ifa);
        }
    }
    dn_recv_timeout(s, 200);
    return s;
}

static void *discovery_thread(void *arg)
{
    dn_sock s = beacon_socket();
    int64_t next_beacon = 0, next_static = 0;
    (void) arg;
    if (s == DN_INVALID)
        logmsg(0, "no discovery socket (UDP %d): listed peers only", cfg.beacon_port);
    while (!stopping) {
        int64_t now = dn_mono_ns();
        if (now >= next_beacon) {
            next_beacon = now + CATALOG_TICK_MS * 1000000LL;
            scan_local();
            if (s != DN_INVALID && cfg.multicast) {
                char msg[256];
                struct sockaddr_in to;
                uint64_t v;
                dn_lock(&catalog_lock);
                v = catalog_version;
                dn_unlock(&catalog_lock);
                snprintf(msg, sizeof msg, "DAONET 1 domain=%s node=%s port=%d catalog=%llu\n",
                         cfg.domain, cfg.node, cfg.data_port, (unsigned long long) v);
                memset(&to, 0, sizeof to);
                to.sin_family = AF_INET;
                to.sin_port = htons((unsigned short) cfg.beacon_port);
                to.sin_addr.s_addr = inet_addr(cfg.group);
                sendto(s, msg, (int) strlen(msg), 0, (struct sockaddr *) &to, sizeof to);
            }
            expire_peers();
        }
        if (now >= next_static) {                      /* listed peers: asked directly */
            int i;
            next_static = now + 2000LL * 1000000LL;
            for (i = 0; i < cfg.n_peers; i++) {
                char host[128], node[64];
                int port = cfg.data_port;
                uint64_t v;
                ShmInfo *shms = NULL;
                int n = 0;
                char *c;
                snprintf(host, sizeof host, "%s", cfg.peers[i]);
                if ((c = strchr(host, ':')) != NULL) {
                    port = atoi(c + 1);
                    *c = '\0';
                }
                if (fetch_catalog(host, port, node, sizeof node, &v, &shms, &n) == 0) {
                    free(shms);
                    if (!(strcmp(node, cfg.node) == 0 && port == cfg.data_port))
                        peer_seen(node, host, port, v, 1);
                }
            }
        }
        if (s != DN_INVALID) {
            char msg[512];
            struct sockaddr_in from;
            socklen_t fl = sizeof from;
            int n = (int) recvfrom(s, msg, sizeof msg - 1, 0, (struct sockaddr *) &from, &fl);
            if (n > 0) {
                char dom[64], node[64], ip[64];
                int port;
                unsigned long long v;
                msg[n] = '\0';
                snprintf(ip, sizeof ip, "%s", inet_ntoa(from.sin_addr));
                if (sscanf(msg, "DAONET 1 domain=%63s node=%63s port=%d catalog=%llu", dom, node, &port, &v) == 4
                    && strcmp(dom, cfg.domain) == 0 && address_allowed(ip)
                    && !(strcmp(node, cfg.node) == 0 && port == cfg.data_port))
                    peer_seen(node, ip, port, v, 0);
            }
        } else {
            dn_sleep_ms(200);
        }
    }
    dn_close(s);
    return NULL;
}

/* ========================================================================== */
/* Owner side: a Source per SHM read by other machines, a Session per reader  */
/* ========================================================================== */

typedef struct Source {
    char     name[256], path[1024];
    IMAGE    img;
    size_t   bytes;
    uint64_t stamp;
    dn_mutex m;                       /* guards cnt, meta_gen, refs; the image while reopened */
    dn_cond  c;
    uint64_t cnt;                     /* the newest counter seen                   */
    uint64_t meta_gen;                /* bumped when the SHM is recreated          */
    int      refs;
    int      broken;                  /* recreated, and cannot be reopened: sessions leave */
    IMAGE    retired[4];              /* earlier mappings, kept until the source ends:     */
    int      n_retired;               /*   --spin sessions read the counter without lock  */
    dn_mutex write_lock;              /* writes coming from readers                */
    struct Source *next;
} Source;

static dn_mutex sources_lock;
static Source *sources = NULL;

static uint64_t source_counter(Source *src)
{
    volatile IMAGE_METADATA *md = (volatile IMAGE_METADATA *) src->img.md;
    uint32_t last = md[0].fifo_last_written;
    return md[last < md[0].fifo_size ? last : 0].cnt0;
}

/** The newest frame, consistently: 0 with cnt and atime set, or -1 when the writer wrote
 * during each of a few attempts (the caller then waits for its next frame: no spinning). */
static int copy_frame(Source *src, void *buf, uint64_t *cnt, int64_t *atime)
{
    volatile IMAGE_METADATA *md = (volatile IMAGE_METADATA *) src->img.md;
    int tries;
    for (tries = 0; tries < 3; tries++) {
        uint32_t idx = md[0].fifo_last_written;
        uint64_t c1, c2;
        if (idx >= md[0].fifo_size)
            idx = 0;
        c1 = md[idx].cnt0;
        MEMORY_BARRIER();
        if (md[idx].write)
            continue;
        memcpy(buf, (const char *) src->img.array.V + (size_t) idx * src->bytes, src->bytes);
        MEMORY_BARRIER();
        c2 = md[idx].cnt0;
        if (c1 == c2 && !md[idx].write && md[0].fifo_last_written == idx) {
            *cnt = c1;
            *atime = md[idx].atime.tsfixed.secondlong;
            return 0;
        }
    }
    return -1;
}

/** Open the source's SHM into img (a CPU SHM): 0, or -1 with img untouched. */
static int open_source_image(Source *src, IMAGE *img)
{
    IMAGE fresh;
    ShmInfo info;
    memset(&fresh, 0, sizeof fresh);
    if (read_shm_header(src->path, &info) != 0 || info.gpu)
        return -1;                                     /* checked first: daoShmOpen exits on some */
    if (daoShmOpen(src->path, &fresh) != DAO_SUCCESS)
        return -1;
    if (daoShmIsGpu(&fresh)) {
        daoShmClose(&fresh);
        return -1;
    }
    *img = fresh;
    src->bytes = (size_t) fresh.md[0].nelement * atype_size(fresh.md[0].atype);
    src->stamp = shm_stamp(src->path);
    return 0;
}

/** Keep the current mapping alive (a --spin session may still read it). src->m held. */
static void retire_image(Source *src)
{
    if (src->n_retired == (int) (sizeof src->retired / sizeof src->retired[0])) {
        daoShmClose(&src->retired[0]);                  /* the oldest: long unused */
        memmove(&src->retired[0], &src->retired[1], sizeof src->retired - sizeof src->retired[0]);
        src->n_retired--;
    }
    src->retired[src->n_retired++] = src->img;
}

/** Wakes the sessions on each new frame: one thread per Source, asleep on DAO_NET_SEM. */
static void *source_thread(void *arg)
{
    Source *src = (Source *) arg;
    int64_t next_check = 0;
    for (;;) {
        struct timespec until;
        int64_t t = dn_real_ns() + 200000000LL;
        uint64_t cnt;
        dn_lock(&src->m);
        if (src->refs == 0) {                            /* the last session let go (even when */
            dn_unlock(&src->m);                          /* stopping: never freed under one)   */
            break;
        }
        dn_unlock(&src->m);
        if (stopping) {
            dn_sleep_ms(20);                             /* its sessions are ending */
            continue;
        }
        until.tv_sec = (time_t) (t / 1000000000LL);
        until.tv_nsec = (long) (t % 1000000000LL);
        if (src->broken)
            dn_sleep_ms(200);                             /* its sessions are leaving */
        else if (src->img.md[0].sem <= DAO_NET_SEM
                 || daoShmWaitSemTimeout(&src->img, DAO_NET_SEM, &until) == DAO_ERROR)
            dn_sleep_ms(1);                               /* no such semaphore: a short sleep instead */
        cnt = source_counter(src);
        dn_lock(&src->m);
        if (cnt != src->cnt) {
            src->cnt = cnt;
            dn_cond_broadcast(&src->c);
        }
        dn_unlock(&src->m);
        if (dn_mono_ns() >= next_check) {                /* recreated with another shape? */
            next_check = dn_mono_ns() + 1000000000LL;
            if (!src->broken && dn_file_exists(src->path) && shm_stamp(src->path) != src->stamp) {
                IMAGE fresh;
                dn_lock(&src->m);
                if (open_source_image(src, &fresh) == 0) {
                    retire_image(src);
                    src->img = fresh;
                    src->cnt = source_counter(src);
                    src->meta_gen++;
                    logmsg(0, "%s: recreated, readers updated", src->name);
                } else {
                    src->broken = 1;                      /* the old mapping stays: nothing reads garbage */
                    logmsg(0, "%s: recreated, cannot reopen it: its readers reconnect", src->name);
                }
                dn_cond_broadcast(&src->c);
                dn_unlock(&src->m);
            }
        }
    }
    /* the last session left: forget it */
    dn_lock(&sources_lock);
    {
        Source **pp = &sources;
        while (*pp && *pp != src)
            pp = &(*pp)->next;
        if (*pp)
            *pp = src->next;
    }
    dn_unlock(&sources_lock);
    daoShmClose(&src->img);
    while (src->n_retired > 0)
        daoShmClose(&src->retired[--src->n_retired]);
    dn_cond_destroy(&src->c);
    dn_mutex_destroy(&src->m);
    dn_mutex_destroy(&src->write_lock);
    logmsg(1, "%s: no reader left", src->name);
    free(src);
    return NULL;
}

static Source *source_get(const char *name)
{
    Source *src;
    dn_lock(&sources_lock);
    for (src = sources; src; src = src->next)
        if (strcmp(src->name, name) == 0) {
            dn_lock(&src->m);
            if (src->refs > 0 && !src->broken) {
                src->refs++;
                dn_unlock(&src->m);
                dn_unlock(&sources_lock);
                return src;
            }
            dn_unlock(&src->m);
        }
    src = (Source *) calloc(1, sizeof *src);
    if (src) {
        snprintf(src->name, sizeof src->name, "%s", name);
        shm_path(src->path, sizeof src->path, "", name);
        if (open_source_image(src, &src->img) != 0) {
            free(src);
            src = NULL;
        } else {
            dn_mutex_init(&src->m);
            dn_cond_init(&src->c);
            dn_mutex_init(&src->write_lock);
            src->refs = 1;
            src->cnt = source_counter(src);
            src->next = sources;
            sources = src;
            if (dn_thread(source_thread, src) != 0) {
                sources = src->next;
                daoShmClose(&src->img);
                free(src);
                src = NULL;
            }
        }
    }
    dn_unlock(&sources_lock);
    return src;
}

static void source_put(Source *src)
{
    dn_lock(&src->m);
    src->refs--;
    dn_cond_broadcast(&src->c);
    dn_unlock(&src->m);
}

typedef struct {
    Source  *src;
    dn_sock  s;
    char     reader[64], ip[64];
    volatile int done;
    volatile uint64_t echo_cnt;       /* the counter our last write-back produced */
    int      readers_alive;           /* the upstream thread still running        */
    dn_mutex m;
} Session;

static DaoNetHeader make_header(uint16_t type, uint32_t flags, uint64_t cnt, uint64_t seq,
                               int64_t atime, uint64_t size, uint32_t dropped)
{
    DaoNetHeader h;
    memset(&h, 0, sizeof h);
    h.magic = DAO_NET_MAGIC;
    h.version = DAO_NET_VERSION;
    h.type = type;
    h.flags = flags;
    h.cnt0 = cnt;
    h.seq = seq;
    h.atime_ns = atime;
    h.size = size;
    h.dropped = dropped;
    return h;
}

static int send_header(dn_sock s, uint16_t type, uint32_t flags, uint64_t cnt, uint64_t seq,
                       int64_t atime, uint64_t size, uint32_t dropped)
{
    DaoNetHeader h = make_header(type, flags, cnt, seq, atime, size, dropped);
    return dn_send_all(s, &h, sizeof h);
}

/** DAO_NET_META: the shape, the type and the keywords of the source. */
static int send_meta(dn_sock s, Source *src, uint64_t seq)
{
    IMAGE_METADATA *md = src->img.md;
    DaoNetMeta meta;
    DaoNetKeyword *kw = NULL;
    int i, rc;
    memset(&meta, 0, sizeof meta);
    snprintf(meta.name, sizeof meta.name, "%s", src->name);
    meta.naxis = md[0].naxis;
    meta.atype = md[0].atype;
    meta.nbkw = md[0].NBkw;
    for (i = 0; i < 3; i++)
        meta.size[i] = i < md[0].naxis ? md[0].size[i] : 1;
    meta.nelement = md[0].nelement;
    meta.elsize = atype_size(md[0].atype);
    if (meta.nbkw) {
        kw = (DaoNetKeyword *) calloc(meta.nbkw, sizeof *kw);
        if (!kw)
            return -1;
        for (i = 0; i < meta.nbkw; i++) {
            memcpy(kw[i].name, src->img.kw[i].name, sizeof kw[i].name);
            kw[i].type = src->img.kw[i].type;
            memcpy(kw[i].value, &src->img.kw[i].value, sizeof kw[i].value);
            memcpy(kw[i].comment, src->img.kw[i].comment, sizeof kw[i].comment);
        }
    }
    rc = send_header(s, DAO_NET_META, 0, 0, seq, 0, sizeof meta + meta.nbkw * sizeof *kw, 0) == 0
         && dn_send_all(s, &meta, sizeof meta) == 0
         && (meta.nbkw == 0 || dn_send_all(s, kw, meta.nbkw * sizeof *kw) == 0) ? 0 : -1;
    free(kw);
    return rc;
}

/** A reader's messages: its writes (written into the source), heartbeats, BYE. */
static void *session_upstream(void *arg)
{
    Session *ss = (Session *) arg;
    Source *src = ss->src;
    void *buf = NULL;
    size_t cap = 0;
    while (!ss->done) {
        DaoNetHeader h;
        if (dn_recv_all(ss->s, &h, sizeof h) != 0 || h.magic != DAO_NET_MAGIC)
            break;
        if (h.type == DAO_NET_BYE)
            break;
        if (h.type != DAO_NET_WRITE) {
            if (h.size > 0)
                break;                                  /* unexpected payload: resynchronising is not worth it */
            continue;
        }
        if (h.size > (uint64_t) src->bytes + 65536)
            break;                                      /* not a frame of this SHM */
        if (h.size > cap) {
            void *grown = realloc(buf, (size_t) h.size);
            if (!grown)
                break;
            buf = grown;
            cap = (size_t) h.size;
        }
        if (dn_recv_all(ss->s, buf, (size_t) h.size) != 0)
            break;
        if (cfg.read_only) {
            logmsg(1, "%s: write from %s refused (--read-only)", src->name, ss->reader);
            continue;
        }
        dn_lock(&src->write_lock);
        dn_lock(&src->m);
        if (h.size == src->bytes) {
            daoShmSetData(&src->img, buf, (uint32_t) src->img.md[0].nelement);
            ss->echo_cnt = source_counter(src);        /* its echo: not published again there */
        }
        dn_unlock(&src->m);
        dn_unlock(&src->write_lock);
    }
    free(buf);
    dn_lock(&ss->m);
    ss->done = 1;
    ss->readers_alive = 0;
    dn_unlock(&ss->m);
    dn_lock(&src->m);
    dn_cond_broadcast(&src->c);                         /* wake the sender */
    dn_unlock(&src->m);
    return NULL;
}

/** Streams a source to one reader: the newest frame each time, heartbeats when idle. */
static void run_session(Session *ss)
{
    Source *src = ss->src;
    uint64_t last = (uint64_t) -1, missed = (uint64_t) -1, seq = 0, meta_gen, dropped = 0;
    int64_t last_send = dn_mono_ns();
    size_t cap = src->bytes, nb = 0, kw_bytes;
    void *buf = malloc(cap ? cap : 1);
    IMAGE_KEYWORD *kw_sent = NULL;
    int ok = buf != NULL;

    dn_lock(&src->m);
    meta_gen = src->meta_gen;
    kw_bytes = (size_t) src->img.md[0].NBkw * sizeof(IMAGE_KEYWORD);
    kw_sent = (IMAGE_KEYWORD *) malloc(kw_bytes ? kw_bytes : 1);
    ok = ok && kw_sent && send_meta(ss->s, src, seq++) == 0;
    if (ok && kw_bytes)
        memcpy(kw_sent, src->img.kw, kw_bytes);
    dn_unlock(&src->m);
    while (ok && !ss->done && !stopping) {
        uint64_t cnt;
        int64_t atime;
        int fresh;
        if (cfg.spin) {                                 /* low latency: poll the SHM's own counter */
            int64_t spins = 0;
            while (!ss->done && !stopping) {
                uint64_t now_cnt = source_counter(src);
                if (now_cnt != last && now_cnt != missed)
                    break;
                dn_cpu_relax();
                if ((++spins & 0xFFF) == 0 && (dn_mono_ns() - last_send >= HEARTBEAT_NS
                                               || src->meta_gen != meta_gen || src->broken))
                    break;
            }
            dn_lock(&src->m);
            src->cnt = source_counter(src);
            dn_unlock(&src->m);
        }
        dn_lock(&src->m);
        /* asleep until a frame newer than the last sent (and than one it could not copy) */
        while ((src->cnt == last || src->cnt == missed) && src->meta_gen == meta_gen && !ss->done
               && !stopping && !src->broken && dn_mono_ns() - last_send < HEARTBEAT_NS)
            dn_cond_wait_ms(&src->c, &src->m, 250);
        if (src->broken) {                              /* cannot be reopened: the reader reconnects */
            dn_unlock(&src->m);
            break;
        }
        if (src->meta_gen != meta_gen) {               /* the SHM was recreated */
            meta_gen = src->meta_gen;
            if (src->bytes > cap) {
                void *grown = realloc(buf, src->bytes);
                if (!grown) {
                    dn_unlock(&src->m);
                    break;
                }
                buf = grown;
                cap = src->bytes;
            }
            kw_bytes = (size_t) src->img.md[0].NBkw * sizeof(IMAGE_KEYWORD);
            free(kw_sent);
            kw_sent = (IMAGE_KEYWORD *) malloc(kw_bytes ? kw_bytes : 1);
            ok = kw_sent && send_meta(ss->s, src, seq++) == 0;
            if (ok && kw_bytes)
                memcpy(kw_sent, src->img.kw, kw_bytes);
            last = (uint64_t) -1;
        }
        fresh = ok && src->cnt != last && src->cnt != missed;
        missed = src->cnt;                              /* if this copy fails: wait past it */
        dn_unlock(&src->m);
        if (!ok || ss->done || stopping)
            break;
        if (!fresh) {                                   /* idle: tell it the link lives */
            ok = send_header(ss->s, DAO_NET_HEARTBEAT, 0, 0, seq++, 0, 0, 0) == 0;
            last_send = dn_mono_ns();
            continue;
        }
        dn_lock(&src->m);
        fresh = copy_frame(src, buf, &cnt, &atime) == 0;
        nb = src->bytes;
        if (fresh && kw_bytes && memcmp(kw_sent, src->img.kw, kw_bytes) != 0) {   /* keywords changed */
            memcpy(kw_sent, src->img.kw, kw_bytes);
            ok = send_meta(ss->s, src, seq++) == 0;
        }
        dn_unlock(&src->m);
        if (!ok)
            break;
        if (!fresh)
            continue;                                   /* the writer was writing: its next frame */
        missed = (uint64_t) -1;
        if (last != (uint64_t) -1 && cnt > last + 1)
            dropped += cnt - last - 1;
        {
            /* header, frame and trailer in one system call */
            DaoNetHeader h = make_header(DAO_NET_FRAME, cnt == ss->echo_cnt ? DAO_NET_FLAG_ECHO : 0, cnt,
                                         seq++, atime, nb, (uint32_t) dropped);
            const void *parts[3] = {&h, buf, &cnt};
            size_t lens[3] = {sizeof h, nb, sizeof cnt};
            ok = dn_send_parts(ss->s, parts, lens, 3) == 0;
        }
        last = cnt;
        last_send = dn_mono_ns();
    }
    if (ok)
        send_header(ss->s, DAO_NET_BYE, 0, 0, seq, 0, 0, 0);
    free(kw_sent);
    free(buf);
}

/* A connection on the data port: a list request, or a reader subscribing. */
static void *data_client_thread(void *arg)
{
    Session *ss = (Session *) arg;
    char line[512], name[256], reader[64];
    dn_recv_timeout(ss->s, 3000);
    if (dn_recv_line(ss->s, line, sizeof line) != 0)
        goto done;
    if (strcmp(line, "CATALOG") == 0) {
        char out[512];
        int i;
        uint64_t v;
        dn_send_timeout(ss->s, 2000);                   /* a slow peer cannot hold the list */
        dn_lock(&catalog_lock);
        v = catalog_version;
        snprintf(out, sizeof out, "NODE %s %llu %s\n", cfg.node, (unsigned long long) v, cfg.domain);
        dn_send_str(ss->s, out);
        for (i = 0; i < n_catalog; i++) {
            ShmInfo *c = &catalog[i];
            snprintf(out, sizeof out, "SHM %s %u %u %u %u %u %u %u %d\n", c->name, c->naxis, c->size[0],
                     c->size[1], c->size[2], c->atype, c->nbkw, c->fifo, c->gpu);
            if (dn_send_str(ss->s, out) != 0)
                break;
        }
        dn_unlock(&catalog_lock);
        dn_send_str(ss->s, "END\n");
        goto done;
    }
    if (sscanf(line, "SUBSCRIBE %255s %63s", name, reader) == 2) {
        ShmInfo info;
        if (!local_has(name, &info)) {
            dn_send_str(ss->s, "ERR no such SHM here\n");
            goto done;
        }
        if (info.gpu) {
            dn_send_str(ss->s, "ERR a GPU SHM: not shared over the network yet\n");
            goto done;
        }
        ss->src = source_get(name);
        if (!ss->src) {
            dn_send_str(ss->s, "ERR cannot open it\n");
            goto done;
        }
        snprintf(ss->reader, sizeof ss->reader, "%s", reader);
        dn_recv_timeout(ss->s, LINK_TIMEOUT_MS);       /* the reader's heartbeats: every second */
        dn_send_timeout(ss->s, LINK_TIMEOUT_MS);       /* a reader that stopped reading */
        if (dn_send_str(ss->s, "OK\n") == 0) {
            logmsg(0, "%s -> %s (%s)", name, reader, ss->ip);
            ss->readers_alive = 1;
            if (dn_thread(session_upstream, ss) != 0)
                ss->readers_alive = 0;
            run_session(ss);
            ss->done = 1;
            dn_shutdown(ss->s);                         /* ends the upstream thread */
            while (1) {                                 /* let it go before freeing ss */
                int alive;
                dn_lock(&ss->m);
                alive = ss->readers_alive;
                dn_unlock(&ss->m);
                if (!alive)
                    break;
                dn_sleep_ms(10);
            }
            logmsg(0, "%s -> %s: closed", name, reader);
        }
        source_put(ss->src);
        goto done;
    }
    dn_send_str(ss->s, "ERR unknown request\n");
done:
    dn_close(ss->s);
    dn_mutex_destroy(&ss->m);
    free(ss);
    return NULL;
}

static void *data_listener(void *arg)
{
    dn_sock l = *(dn_sock *) arg;
    while (!stopping) {
        struct sockaddr_in from;
        socklen_t fl = sizeof from;
        dn_sock s = accept(l, (struct sockaddr *) &from, &fl);
        Session *ss;
        char ip[64];
        if (s == DN_INVALID) {
            if (!stopping)
                dn_sleep_ms(10);
            continue;
        }
        snprintf(ip, sizeof ip, "%s", inet_ntoa(from.sin_addr));
        if (!address_allowed(ip)) {
            logmsg(0, "refused %s (--allow)", ip);
            dn_close(s);
            continue;
        }
        dn_tune(s);
        ss = (Session *) calloc(1, sizeof *ss);
        if (!ss) {
            dn_close(s);
            continue;
        }
        ss->s = s;
        ss->echo_cnt = (uint64_t) -1;
        snprintf(ss->ip, sizeof ss->ip, "%s", ip);
        dn_mutex_init(&ss->m);
        if (dn_thread(data_client_thread, ss) != 0) {
            dn_close(s);
            dn_mutex_destroy(&ss->m);
            free(ss);
        }
    }
    return NULL;
}

/* ========================================================================== */
/* Reader side: a Replica per remote SHM used here                            */
/* ========================================================================== */

enum { R_CONNECTING = 0, R_READY = 1, R_DOWN = 2 };

typedef struct Replica {
    char     node[64], name[256], key[330], path[1024];
    IMAGE    img;
    int      created;
    size_t   bytes;
    dn_sock  s;
    dn_mutex m;                       /* guards the image, my_cnt, state, the socket's sends */
    dn_cond  c;
    int      state, keep, stop, threads;
    int      users;                   /* requests waiting on it: never destroyed under them */
    IMAGE    retired[4];              /* images replaced by a new shape, kept open until the */
    int      n_retired;               /*   replica ends: the watch thread may wait on them    */
    uint64_t my_cnt;                  /* the counter our last publication left      */
    uint64_t frames, dropped, src_cnt;
    int64_t  src_time, age_us, used_ns, last_frame_ns;   /* used_ns: last known in use */
    double   rate_hz;
    struct Replica *next;
} Replica;

static dn_mutex replicas_lock;
static Replica *replicas = NULL;

static uint64_t replica_counter(Replica *r)
{
    return ((volatile IMAGE_METADATA *) r->img.md)[0].cnt0;
}

static IMAGE_KEYWORD *net_kw(Replica *r, const char *kname)
{
    int i;
    for (i = 0; i < r->img.md[0].NBkw; i++)
        if (strncmp(r->img.kw[i].name, kname, sizeof r->img.kw[i].name) == 0)
            return &r->img.kw[i];
    return NULL;
}

static void set_kw_long(Replica *r, const char *kname, int64_t v)
{
    IMAGE_KEYWORD *k = net_kw(r, kname);
    if (k)
        k->value.numl = v;
}

/** (Re)create the replica for this shape and type: 0, or -1. r->m held. */
static int replica_create(Replica *r, const DaoNetMeta *meta, const DaoNetKeyword *kws)
{
    uint32_t size[3];
    int i, nbkw = meta->nbkw + NET_KW_COUNT;
    IMAGE_KEYWORD *k;
    static const char *names[NET_KW_COUNT] = {"NET_REPLICA", "NET_SRC_CNT", "NET_SRC_TIME",
                                              "NET_AGE_US", "NET_DROPPED", "NET_LINK"};
    static const char *comments[NET_KW_COUNT] = {
        "a network replica of the SHM of this machine", "the source's SHM counter of the last frame",
        "the last frame's write time on the source, UTC ns", "its age here, us (clocks synchronised)",
        "frames the owner skipped (latest-frame streaming)", "1: the owner is connected, 0: not"};
    if (r->created) {
        IMAGE_METADATA *md = r->img.md;
        int same = md[0].atype == meta->atype && md[0].naxis == meta->naxis
                   && md[0].NBkw == nbkw && md[0].nelement == meta->nelement;
        for (i = 0; same && i < meta->naxis; i++)
            same = md[0].size[i] == meta->size[i];
        if (same)
            goto keywords;
        /* another shape: the old image stays open (the watch thread may be waiting on
         * its semaphore) until the replica ends; the new one replaces it here */
        if (r->n_retired == (int) (sizeof r->retired / sizeof r->retired[0])) {
            daoShmClose(&r->retired[0]);
            memmove(&r->retired[0], &r->retired[1], sizeof r->retired - sizeof r->retired[0]);
            r->n_retired--;
        }
        r->retired[r->n_retired++] = r->img;
        r->created = 0;
    }
    for (i = 0; i < 3; i++)
        size[i] = meta->size[i];
    memset(&r->img, 0, sizeof r->img);
    if (daoShmCreate(&r->img, r->path, meta->naxis, size, meta->atype, 1, nbkw) != DAO_SUCCESS)
        return -1;
    r->created = 1;
    r->bytes = (size_t) meta->nelement * (size_t) meta->elsize;
    r->my_cnt = replica_counter(r);
    for (i = 0; i < NET_KW_COUNT; i++) {
        k = &r->img.kw[meta->nbkw + i];
        memset(k, 0, sizeof *k);
        snprintf(k->name, sizeof k->name, "%s", names[i]);
        snprintf(k->comment, sizeof k->comment, "%s", comments[i]);
        k->type = 'L';
    }
    k = &r->img.kw[meta->nbkw];
    k->type = 'S';
    snprintf(k->value.valstr, sizeof k->value.valstr, "%.15s", r->node);   /* 15 characters: the value's room */
keywords:
    for (i = 0; i < meta->nbkw; i++) {                  /* the source's own keywords */
        k = &r->img.kw[i];
        memcpy(k->name, kws[i].name, sizeof k->name);
        k->type = kws[i].type;
        memcpy(&k->value, kws[i].value, sizeof kws[i].value);
        memcpy(k->comment, kws[i].comment, sizeof k->comment);
    }
    return 0;
}

/** A local write on the replica (a counter we did not leave): sent to the owner. r->m held. */
static void forward_local_write(Replica *r)
{
    uint64_t cnt = replica_counter(r);
    void *buf;
    if (!r->created || cnt == r->my_cnt || r->s == DN_INVALID || r->state != R_READY)
        return;
    r->my_cnt = cnt;
    buf = malloc(r->bytes ? r->bytes : 1);
    if (!buf)
        return;
    memcpy(buf, r->img.array.V, r->bytes);
    if (send_header(r->s, DAO_NET_WRITE, 0, cnt, 0, dn_real_ns(), r->bytes, 0) != 0
        || dn_send_all(r->s, buf, r->bytes) != 0)
        dn_shutdown(r->s);
    free(buf);
}

/** One connection to the owner, until it breaks: 0 if it ever became ready. */
static int replica_session(Replica *r, const char *addr, int port)
{
    char line[512];
    int ever_ready = 0;
    DaoNetMeta meta;
    DaoNetKeyword *kws = NULL;
    dn_sock s = dn_connect(addr, port, 1000);
    if (s == DN_INVALID)
        return -1;
    snprintf(line, sizeof line, "SUBSCRIBE %s %s\n", r->name, cfg.node);
    dn_recv_timeout(s, LINK_TIMEOUT_MS);
    dn_send_timeout(s, LINK_TIMEOUT_MS);
    if (dn_send_str(s, line) != 0 || dn_recv_line(s, line, sizeof line) != 0 || strcmp(line, "OK") != 0) {
        if (strncmp(line, "ERR", 3) == 0)
            logmsg(0, "%s: %s", r->key, line);
        dn_close(s);
        return -1;
    }
    dn_lock(&r->m);
    r->s = s;
    dn_unlock(&r->m);
    for (;;) {
        DaoNetHeader h;
        if (cfg.spin && dn_spin_readable(s, LINK_TIMEOUT_MS) != 1)   /* low latency: no sleep in recv */
            break;
        if (r->stop || dn_recv_all(s, &h, sizeof h) != 0 || h.magic != DAO_NET_MAGIC || h.type == DAO_NET_BYE)
            break;
        if (h.type == DAO_NET_HEARTBEAT)
            continue;
        if (h.type == DAO_NET_META) {
            size_t nk;
            if (h.size < sizeof meta || dn_recv_all(s, &meta, sizeof meta) != 0)
                break;
            nk = (size_t) (h.size - sizeof meta) / sizeof(DaoNetKeyword);
            if (nk != meta.nbkw || meta.elsize != atype_size(meta.atype) || meta.naxis < 1 || meta.naxis > 3)
                break;
            free(kws);
            kws = (DaoNetKeyword *) calloc(nk ? nk : 1, sizeof *kws);
            if (!kws || (nk && dn_recv_all(s, kws, nk * sizeof *kws) != 0))
                break;
            dn_lock(&r->m);
            if (replica_create(r, &meta, kws) != 0) {
                dn_unlock(&r->m);
                logmsg(0, "%s: cannot create %s", r->key, r->path);
                break;
            }
            dn_unlock(&r->m);
            continue;
        }
        if (h.type != DAO_NET_FRAME || !r->created || h.size != r->bytes)
            break;
        {
            uint64_t trailer;
            volatile IMAGE_METADATA *md;
            int publish;
            dn_lock(&r->m);
            forward_local_write(r);                     /* a local write first: never lost */
            md = (volatile IMAGE_METADATA *) r->img.md;
            md[0].write = 1;
            dn_unlock(&r->m);
            /* straight into the replica (no copy on this side), with its trailer: one call */
            {
                void *parts[2] = {r->img.array.V, &trailer};
                size_t lens[2] = {r->bytes, sizeof trailer};
                if (dn_recv_parts(s, parts, lens, 2) != 0) {
                    md[0].write = 0;
                    break;
                }
            }
            dn_lock(&r->m);
            publish = trailer == h.cnt0 && !(h.flags & DAO_NET_FLAG_ECHO);
            if (publish) {
                int64_t now = dn_real_ns();
                set_kw_long(r, "NET_SRC_CNT", (int64_t) h.cnt0);
                set_kw_long(r, "NET_SRC_TIME", h.atime_ns);
                set_kw_long(r, "NET_AGE_US", h.atime_ns ? (now - h.atime_ns) / 1000 : 0);
                set_kw_long(r, "NET_DROPPED", h.dropped);
                set_kw_long(r, "NET_LINK", 1);
                daoShmSetDataPartFinalize(&r->img);     /* counter + 1, every semaphore posted */
                r->my_cnt = replica_counter(r);
                if (r->last_frame_ns) {
                    double dt = (double) (dn_mono_ns() - r->last_frame_ns) * 1e-9;
                    if (dt > 0)
                        r->rate_hz = r->rate_hz > 0 ? 0.9 * r->rate_hz + 0.1 / dt : 1.0 / dt;
                }
                r->last_frame_ns = dn_mono_ns();
                r->frames++;
                r->dropped = h.dropped;
                r->src_cnt = h.cnt0;
                r->src_time = h.atime_ns;
                r->age_us = h.atime_ns ? (now - h.atime_ns) / 1000 : 0;
                if (r->state != R_READY) {
                    r->state = R_READY;
                    dn_cond_broadcast(&r->c);
                }
                ever_ready = 1;
            } else {
                md[0].write = 0;                        /* our own write, back: already here */
            }
            dn_unlock(&r->m);
        }
    }
    free(kws);
    dn_lock(&r->m);
    r->s = DN_INVALID;
    if (r->created)
        set_kw_long(r, "NET_LINK", 0);
    if (r->state == R_READY)
        r->state = R_DOWN;
    dn_cond_broadcast(&r->c);
    dn_unlock(&r->m);
    dn_close(s);
    return ever_ready ? 0 : -1;
}

/** Keeps the replica connected to its owner, reconnecting while it is wanted. */
static void *replica_thread(void *arg)
{
    Replica *r = (Replica *) arg;
    int backoff = 250;
    while (!r->stop && !stopping) {
        char addr[64], owner[64];
        int port;
        if (find_remote(r->node, r->name, addr, sizeof addr, &port, owner, sizeof owner, NULL) == 0) {
            if (replica_session(r, addr, port) == 0) {
                backoff = 250;
                if (!r->stop)
                    logmsg(0, "%s: link lost, reconnecting", r->key);
            }
        }
        if (!r->stop)
            dn_sleep_ms(backoff);
        backoff = backoff < 2000 ? 2 * backoff : 2000;
    }
    dn_lock(&r->m);
    r->threads--;
    dn_cond_broadcast(&r->c);
    dn_unlock(&r->m);
    return NULL;
}

/** Local writes (forwarded to the owner) and heartbeats: asleep on the replica's DAO_NET_SEM. */
static void *replica_watch_thread(void *arg)
{
    Replica *r = (Replica *) arg;
    int64_t last_beat = 0;
    while (!r->stop && !stopping) {
        int created;
        dn_lock(&r->m);
        created = r->created;
        dn_unlock(&r->m);
        if (!created) {
            dn_sleep_ms(50);
            continue;
        }
        {
            /* a copy of the image, taken under the lock: a new shape replaces r->img, while
             * the old one stays open until the replica ends (retired) */
            struct timespec until;
            int64_t t = dn_real_ns() + 250000000LL;
            IMAGE img;
            dn_lock(&r->m);
            img = r->img;
            dn_unlock(&r->m);
            until.tv_sec = (time_t) (t / 1000000000LL);
            until.tv_nsec = (long) (t % 1000000000LL);
            if (img.md[0].sem <= DAO_NET_SEM || daoShmWaitSemTimeout(&img, DAO_NET_SEM, &until) == DAO_ERROR)
                dn_sleep_ms(1);
        }
        dn_lock(&r->m);
        forward_local_write(r);
        if (r->s != DN_INVALID && dn_mono_ns() - last_beat > HEARTBEAT_NS) {
            if (send_header(r->s, DAO_NET_HEARTBEAT, 0, 0, 0, 0, 0, 0) != 0)
                dn_shutdown(r->s);
            last_beat = dn_mono_ns();
        }
        dn_unlock(&r->m);
    }
    dn_lock(&r->m);
    r->threads--;
    dn_cond_broadcast(&r->c);
    dn_unlock(&r->m);
    return NULL;
}

static Replica *replica_find(const char *key)
{
    Replica *r;
    for (r = replicas; r; r = r->next)
        if (strcmp(r->key, key) == 0)
            return r;
    return NULL;
}

/** Take r out of the list (replicas_lock held): whoever does it, and only them, destroys it. */
static void replica_unlink(Replica *r)
{
    Replica **pp = &replicas;
    while (*pp && *pp != r)
        pp = &(*pp)->next;
    if (*pp)
        *pp = r->next;
}

/** Stop an unlinked replica, wait for its threads and requests, free it. */
static void replica_destroy(Replica *r, int remove_file)
{
    dn_lock(&r->m);
    r->stop = 1;
    if (r->s != DN_INVALID) {
        send_header(r->s, DAO_NET_BYE, 0, 0, 0, 0, 0, 0);
        dn_shutdown(r->s);
    }
    dn_cond_broadcast(&r->c);
    while (r->threads > 0 || r->users > 0)
        dn_cond_wait_ms(&r->c, &r->m, 100);
    if (r->created) {
        daoShmClose(&r->img);
        if (remove_file)
            dn_remove(r->path);
    }
    while (r->n_retired > 0)
        daoShmClose(&r->retired[--r->n_retired]);
    dn_unlock(&r->m);
    dn_cond_destroy(&r->c);
    dn_mutex_destroy(&r->m);
    logmsg(0, "%s: released", r->key);
    free(r);
}

/** WANT: the local path of `spec`, waited for up to timeout_ms. 0, or -1 with why. */
static int want(const char *spec, int keep, int timeout_ms, char *path, size_t plen, char *why, size_t wlen)
{
    char node[64], name[256], addr[64], owner[64], key[330];
    int port, where, fresh;
    Replica *r;
    int64_t deadline;
    split_spec(spec, node, sizeof node, name, sizeof name);
    if (!name[0]) {
        snprintf(why, wlen, "no SHM name");
        return -1;
    }
    /* this machine's own SHM: a local one always wins */
    if ((!node[0] || strcmp(node, cfg.node) == 0) && local_has(name, NULL)) {
        shm_path(path, plen, "", name);
        return 0;
    }
    where = find_remote(node, name, addr, sizeof addr, &port, owner, sizeof owner, NULL);
    if (where < 0) {
        snprintf(why, wlen, "%s is on no machine of domain %s", name, cfg.domain);
        return -1;
    }
    if (where > 0) {
        snprintf(why, wlen, "%.200s is on several machines: ask for <machine>:%.200s", name, name);
        return -1;
    }
    snprintf(key, sizeof key, "%s:%s", owner, name);
    dn_lock(&replicas_lock);
    r = replica_find(key);
    if (!r) {
        r = (Replica *) calloc(1, sizeof *r);
        if (!r) {
            dn_unlock(&replicas_lock);
            snprintf(why, wlen, "out of memory");
            return -1;
        }
        snprintf(r->node, sizeof r->node, "%s", owner);
        snprintf(r->name, sizeof r->name, "%s", name);
        snprintf(r->key, sizeof r->key, "%s", key);
        shm_path(r->path, sizeof r->path, cfg.prefix, name);
        r->s = DN_INVALID;
        r->used_ns = dn_mono_ns();
        dn_mutex_init(&r->m);
        dn_cond_init(&r->c);
        r->threads = 2;
        r->next = replicas;
        replicas = r;
        if (dn_thread(replica_thread, r) != 0)
            r->threads--;
        if (dn_thread(replica_watch_thread, r) != 0)
            r->threads--;
        logmsg(0, "%s: replicating into %s", key, r->path);
    }
    r->keep |= keep;
    r->used_ns = dn_mono_ns();
    snprintf(path, plen, "%s", r->path);
    dn_lock(&r->m);
    r->users++;                                         /* not destroyed while we wait */
    dn_unlock(&r->m);
    dn_unlock(&replicas_lock);

    deadline = dn_mono_ns() + (int64_t) timeout_ms * 1000000LL;
    dn_lock(&r->m);
    while (r->state == R_CONNECTING && dn_mono_ns() < deadline)
        dn_cond_wait_ms(&r->c, &r->m, 50);
    where = r->created && r->state != R_CONNECTING;
    fresh = !r->created && !r->keep;
    r->users--;
    dn_cond_broadcast(&r->c);
    dn_unlock(&r->m);
    if (!where) {
        snprintf(why, wlen, "%s did not come within %d ms", key, timeout_ms);
        if (fresh) {                                    /* never came: not kept trying */
            Replica *gone = NULL;
            dn_lock(&replicas_lock);
            if (replica_find(key) == r) {
                dn_lock(&r->m);
                if (r->users == 0 && !r->created) {
                    replica_unlink(r);
                    gone = r;
                }
                dn_unlock(&r->m);
            }
            dn_unlock(&replicas_lock);
            if (gone)
                replica_destroy(gone, 0);
        }
        return -1;
    }
    return 0;
}

static int is_our_replica(const char *path)
{
    Replica *r;
    int found = 0;
    dn_lock(&replicas_lock);
    for (r = replicas; r && !found; r = r->next)
        found = strcmp(r->path, path) == 0;
    dn_unlock(&replicas_lock);
    return found;
}

/* --------------------------------------- replicas nobody maps any more (Linux) */

#ifdef __linux__
/** Is `path` mapped by another process? (/proc/<pid>/maps) */
static int mapped_elsewhere(const char *path)
{
    DIR *d = opendir("/proc");
    struct dirent *e;
    int found = 0;
    pid_t self = getpid();
    if (!d)
        return 1;                                       /* cannot tell: keep it */
    while (!found && (e = readdir(d)) != NULL) {
        char maps[64], line[1536];
        FILE *f;
        long pid = strtol(e->d_name, NULL, 10);
        if (pid <= 0 || pid == self)
            continue;
        snprintf(maps, sizeof maps, "/proc/%ld/maps", pid);
        f = fopen(maps, "r");
        if (!f)
            continue;
        while (!found && fgets(line, sizeof line, f)) {
            char *nl = strchr(line, '\n');
            size_t n, pl = strlen(path);
            if (nl)
                *nl = '\0';
            n = strlen(line);
            if (n >= pl && strcmp(line + n - pl, path) == 0)
                found = 1;
        }
        fclose(f);
    }
    closedir(d);
    return found;
}
#endif

static void *janitor_thread(void *arg)
{
    (void) arg;
    while (!stopping) {
        dn_sleep_ms(2000);
#ifdef __linux__
        if (cfg.idle_s > 0) {
            Replica *r, *idle = NULL;
            int64_t now = dn_mono_ns();
            dn_lock(&replicas_lock);
            for (r = replicas; r && !idle; r = r->next) {
                int busy;
                dn_lock(&r->m);
                busy = r->users > 0;
                dn_unlock(&r->m);
                if (!r->keep && !busy && now - r->used_ns > (int64_t) cfg.idle_s * 1000000000LL) {
                    if (mapped_elsewhere(r->path))
                        r->used_ns = now;                   /* used: counted again from now */
                    else
                        idle = r;
                }
            }
            if (idle)
                replica_unlink(idle);
            dn_unlock(&replicas_lock);
            if (idle) {
                logmsg(0, "%s: unused for %d s", idle->key, cfg.idle_s);
                replica_destroy(idle, 1);
            }
        }
#endif
    }
    return NULL;
}

/* ========================================================================== */
/* Control port (127.0.0.1): the local processes and daoShmNet                */
/* ========================================================================== */

static const char *atype_name(int t)
{
    static const char *names[] = {"?", "uint8", "int8", "uint16", "int16", "uint32", "int32",
                                  "uint64", "int64", "float32", "float64", "complex64", "complex128"};
    return t > 0 && t <= 12 ? names[t] : "?";
}

static void shape_str(const ShmInfo *s, char *out, size_t len)
{
    if (s->naxis == 1)
        snprintf(out, len, "%u", s->size[0]);
    else if (s->naxis == 2)
        snprintf(out, len, "%ux%u", s->size[0], s->size[1]);
    else
        snprintf(out, len, "%ux%ux%u", s->size[0], s->size[1], s->size[2]);
}

static void control_list(dn_sock s)
{
    char out[768], shape[64];
    int i, j;
    dn_lock(&catalog_lock);
    for (i = 0; i < n_catalog; i++) {
        shape_str(&catalog[i], shape, sizeof shape);
        snprintf(out, sizeof out, "SHM\t%s\t%s\t%s\t%s\tlocal%s\n", cfg.node, catalog[i].name, shape,
                 atype_name(catalog[i].atype), catalog[i].gpu ? ",gpu" : "");
        dn_send_str(s, out);
    }
    dn_unlock(&catalog_lock);
    dn_lock(&peers_lock);
    for (i = 0; i < MAX_PEERS; i++)
        for (j = 0; peers[i].used && j < peers[i].n_shms; j++) {
            char key[330];
            Replica *r;
            const char *state = "remote";
            shape_str(&peers[i].shms[j], shape, sizeof shape);
            snprintf(key, sizeof key, "%.63s:%.255s", peers[i].node, peers[i].shms[j].name);
            dn_lock(&replicas_lock);
            r = replica_find(key);
            if (r)
                state = r->state == R_READY ? "replica" : "replica,down";
            dn_unlock(&replicas_lock);
            snprintf(out, sizeof out, "SHM\t%.63s\t%.255s\t%s\t%s\t%s%s\n", peers[i].node, peers[i].shms[j].name, shape,
                     atype_name(peers[i].shms[j].atype), state, peers[i].shms[j].gpu ? ",gpu" : "");
            dn_send_str(s, out);
        }
    dn_unlock(&peers_lock);
}

static void control_status(dn_sock s)
{
    char out[1536];
    Replica *r;
    Source *src;
    int i;
    snprintf(out, sizeof out, "NODE\t%s\t%s\t%d\t%d\t%s\n", cfg.node, cfg.domain, cfg.data_port, cfg.control_port,
             cfg.dir);
    dn_send_str(s, out);
    dn_lock(&peers_lock);
    for (i = 0; i < MAX_PEERS; i++)
        if (peers[i].used) {
            snprintf(out, sizeof out, "PEER\t%.63s\t%.63s\t%d\t%d\t%lld\t%s\n", peers[i].node, peers[i].addr,
                     peers[i].port, peers[i].n_shms, (long long) ((dn_mono_ns() - peers[i].last_seen) / 1000000),
                     peers[i].is_static ? "listed" : "multicast");
            dn_send_str(s, out);
        }
    dn_unlock(&peers_lock);
    dn_lock(&replicas_lock);
    for (r = replicas; r; r = r->next) {
        dn_lock(&r->m);
        snprintf(out, sizeof out, "REPLICA\t%s\t%s\t%s\t%llu\t%llu\t%.1f\t%lld\t%s\n", r->key, r->path,
                 r->state == R_READY ? "up" : r->state == R_DOWN ? "down" : "connecting",
                 (unsigned long long) r->frames, (unsigned long long) r->dropped, r->rate_hz,
                 (long long) r->age_us, r->keep ? "keep" : "auto");
        dn_unlock(&r->m);
        dn_send_str(s, out);
    }
    dn_unlock(&replicas_lock);
    dn_lock(&sources_lock);
    for (src = sources; src; src = src->next) {
        dn_lock(&src->m);
        snprintf(out, sizeof out, "SOURCE\t%s\t%d\t%llu\n", src->name, src->refs, (unsigned long long) src->cnt);
        dn_unlock(&src->m);
        dn_send_str(s, out);
    }
    dn_unlock(&sources_lock);
}

static void *control_client_thread(void *arg)
{
    dn_sock s = *(dn_sock *) arg;
    char line[1024], spec[900], out[1200];
    int timeout_ms;
    free(arg);
    dn_recv_timeout(s, 5000);
    if (dn_recv_line(s, line, sizeof line) != 0) {
        dn_close(s);
        return NULL;
    }
    if (sscanf(line, "WANT %899s %d", spec, &timeout_ms) >= 1 || sscanf(line, "KEEP %899s %d", spec, &timeout_ms) >= 1) {
        char path[1024], why[512];
        int keep = strncmp(line, "KEEP", 4) == 0;
        if (sscanf(line + 5, "%*s %d", &timeout_ms) != 1 || timeout_ms <= 0)
            timeout_ms = DAO_NET_RESOLVE_MS;
        if (want(spec, keep, timeout_ms, path, sizeof path, why, sizeof why) == 0)
            snprintf(out, sizeof out, "OK %s\n", path);
        else
            snprintf(out, sizeof out, "ERR %s\n", why);
        dn_send_str(s, out);
    } else if (sscanf(line, "RELEASE %899s", spec) == 1) {
        char node[64], name[256], key[330];
        Replica *r, *hit = NULL;
        split_spec(spec, node, sizeof node, name, sizeof name);
        dn_lock(&replicas_lock);
        for (r = replicas; r && !hit; r = r->next)
            if (strcmp(r->name, name) == 0 && (!node[0] || strcmp(r->node, node) == 0))
                hit = r;
        if (hit) {
            snprintf(key, sizeof key, "%s", hit->key);
            replica_unlink(hit);
        }
        dn_unlock(&replicas_lock);
        if (hit) {
            replica_destroy(hit, 1);
            snprintf(out, sizeof out, "OK %s released\n", key);
        } else {
            snprintf(out, sizeof out, "ERR no replica of %s here\n", spec);
        }
        dn_send_str(s, out);
    } else if (strcmp(line, "LIST") == 0) {
        control_list(s);
        dn_send_str(s, "END\n");
    } else if (strcmp(line, "STATUS") == 0) {
        control_status(s);
        dn_send_str(s, "END\n");
    } else if (strcmp(line, "PING") == 0) {
        snprintf(out, sizeof out, "OK %s %s %s\n", cfg.node, cfg.domain, NETD_VERSION);
        dn_send_str(s, out);
    } else if (strcmp(line, "QUIT") == 0) {
        dn_send_str(s, "OK stopping\n");
        stopping = 1;
    } else {
        dn_send_str(s, "ERR commands: WANT, KEEP, RELEASE, LIST, STATUS, PING, QUIT\n");
    }
    dn_close(s);
    return NULL;
}

/* ========================================================================== */
/* Main                                                                        */
/* ========================================================================== */

static void on_signal(int sig)
{
    (void) sig;
    stopping = 1;
}

static void usage(void)
{
    fprintf(stderr,
            "daoShmNetd %s: every dao SHM of the network, opened by its name.\n"
            "usage: daoShmNetd [--name NODE] [--domain NAME] [--dir DIR] [--replica-prefix P]\n"
            "                  [--port N] [--control-port N] [--beacon-port N] [--group ADDR]\n"
            "                  [--iface ADDR] [--bind ADDR] [--peer HOST[:PORT]]... [--allow PREFIX]...\n"
            "                  [--no-multicast] [--idle SECONDS] [--read-only] [--spin] [-v]\n"
            "See daoBase docs, \"Network SHMs\", or daoShmNet.py --help.\n", NETD_VERSION);
}

static int parse_args(int argc, char **argv)
{
    int i;
    const char *env;
    memset(&cfg, 0, sizeof cfg);
    dn_hostname(cfg.node, sizeof cfg.node);
    env = getenv("DAO_NET_DOMAIN");
    snprintf(cfg.domain, sizeof cfg.domain, "%s", env && *env ? env : "dao");
    env = getenv("DAO_SHM_DIR");
#ifdef _WIN32
    snprintf(cfg.dir, sizeof cfg.dir, "%s", env && *env ? env : ".");
#else
    snprintf(cfg.dir, sizeof cfg.dir, "%s", env && *env ? env : "/tmp");
#endif
    snprintf(cfg.group, sizeof cfg.group, "%s", DAO_NET_BEACON_GROUP);
    cfg.data_port = DAO_NET_DATA_PORT;
    cfg.control_port = DAO_NET_CONTROL_PORT;
    cfg.beacon_port = DAO_NET_BEACON_PORT;
    cfg.multicast = 1;
    cfg.idle_s = 30;
    for (i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
#define TAKE(dst) do { if (!v) { usage(); return -1; } snprintf(dst, sizeof dst, "%s", v); i++; } while (0)
        if (strcmp(a, "--name") == 0) TAKE(cfg.node);
        else if (strcmp(a, "--domain") == 0) TAKE(cfg.domain);
        else if (strcmp(a, "--dir") == 0) TAKE(cfg.dir);
        else if (strcmp(a, "--replica-prefix") == 0) TAKE(cfg.prefix);
        else if (strcmp(a, "--group") == 0) TAKE(cfg.group);
        else if (strcmp(a, "--iface") == 0) TAKE(cfg.iface);
        else if (strcmp(a, "--bind") == 0) TAKE(cfg.bind_ip);
        else if (strcmp(a, "--port") == 0 && v) { cfg.data_port = atoi(v); i++; }
        else if (strcmp(a, "--control-port") == 0 && v) { cfg.control_port = atoi(v); i++; }
        else if (strcmp(a, "--beacon-port") == 0 && v) { cfg.beacon_port = atoi(v); i++; }
        else if (strcmp(a, "--idle") == 0 && v) { cfg.idle_s = atoi(v); i++; }
        else if (strcmp(a, "--peer") == 0 && v && cfg.n_peers < MAX_LIST) { snprintf(cfg.peers[cfg.n_peers++], 128, "%s", v); i++; }
        else if (strcmp(a, "--allow") == 0 && v && cfg.n_allow < MAX_LIST) { snprintf(cfg.allow[cfg.n_allow++], 64, "%s", v); i++; }
        else if (strcmp(a, "--no-multicast") == 0) cfg.multicast = 0;
        else if (strcmp(a, "--read-only") == 0) cfg.read_only = 1;
        else if (strcmp(a, "--spin") == 0) cfg.spin = 1;
        else if (strcmp(a, "-v") == 0) cfg.verbose = 1;
        else if (strcmp(a, "--version") == 0) { printf("daoShmNetd %s (protocol %u)\n", NETD_VERSION, DAO_NET_VERSION); exit(0); }
        else { usage(); return strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0 ? 1 : -1; }
#undef TAKE
    }
    {
        size_t n = strlen(cfg.dir);
        while (n > 1 && (cfg.dir[n - 1] == '/' || cfg.dir[n - 1] == '\\'))
            cfg.dir[--n] = '\0';
    }
    return 0;
}

int main(int argc, char **argv)
{
    dn_sock data_l, control_l;
    int rc = parse_args(argc, argv);
    const uint16_t one = 1;
    if (rc != 0)
        return rc > 0 ? 0 : 2;
    if (*(const uint8_t *) &one != 1) {
        fprintf(stderr, "daoShmNetd: little-endian machines only (the wire format)\n");
        return 2;
    }
#ifdef _WIN32
    _putenv("DAO_NET=0");                   /* our own opens never ask ourselves */
#else
    setenv("DAO_NET", "0", 1);
    signal(SIGPIPE, SIG_IGN);
#endif
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    daoSetLogLevel(cfg.verbose ? DAO_INFO : DAO_WARNING);
    if (dn_net_init() != 0) {
        fprintf(stderr, "daoShmNetd: cannot start the network\n");
        return 1;
    }
    dn_mutex_init(&catalog_lock);
    dn_mutex_init(&peers_lock);
    dn_mutex_init(&sources_lock);
    dn_mutex_init(&replicas_lock);

    control_l = dn_listen("127.0.0.1", cfg.control_port);
    if (control_l == DN_INVALID) {
        fprintf(stderr, "daoShmNetd: control port %d busy: a service already runs here?\n", cfg.control_port);
        return 1;
    }
    data_l = dn_listen(cfg.bind_ip[0] ? cfg.bind_ip : NULL, cfg.data_port);
    if (data_l == DN_INVALID) {
        fprintf(stderr, "daoShmNetd: data port %d busy\n", cfg.data_port);
        return 1;
    }
    scan_local();
    logmsg(0, "domain %s, SHMs in %s, data port %d, control 127.0.0.1:%d, discovery %s",
           cfg.domain, cfg.dir, cfg.data_port, cfg.control_port,
           cfg.multicast ? cfg.group : "off (listed peers)");
    if (dn_thread(discovery_thread, NULL) != 0 || dn_thread(data_listener, &data_l) != 0
        || dn_thread(janitor_thread, NULL) != 0) {
        fprintf(stderr, "daoShmNetd: cannot start its threads\n");
        return 1;
    }
    dn_recv_timeout(control_l, 0);
    while (!stopping) {
        fd_set r;
        struct timeval tv;
        FD_ZERO(&r);
        FD_SET(control_l, &r);
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        if (select((int) control_l + 1, &r, NULL, NULL, &tv) > 0) {
            dn_sock *c = (dn_sock *) malloc(sizeof *c);
            if (!c)
                continue;
            *c = accept(control_l, NULL, NULL);
            if (*c == DN_INVALID || dn_thread(control_client_thread, c) != 0) {
                dn_close(*c);
                free(c);
            }
        }
    }
    logmsg(0, "stopping");
    dn_close(control_l);
    dn_shutdown(data_l);
    dn_close(data_l);
    {
        /* replicas: their files stay (processes may map them), marked link down */
        Replica *r;
        dn_lock(&replicas_lock);
        for (r = replicas; r; r = r->next) {
            dn_lock(&r->m);
            r->stop = 1;
            if (r->created)
                set_kw_long(r, "NET_LINK", 0);
            if (r->s != DN_INVALID)
                dn_shutdown(r->s);
            dn_unlock(&r->m);
        }
        dn_unlock(&replicas_lock);
    }
    dn_sleep_ms(300);
    return 0;
}
