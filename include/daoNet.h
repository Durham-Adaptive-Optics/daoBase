/**
 * @file    daoNet.h
 * @brief   dao SHMs over the network: every dao SHM of the network, opened by its name.
 *
 * A small service, daoShmNetd, runs on each machine. The services find each other
 * (UDP multicast within a domain, or listed peers) and exchange the lists of their
 * SHMs. When a process opens an SHM that is not on its machine, libdao asks the
 * local service: it subscribes to the SHM's machine and keeps a local replica
 * with the same name up to date -- the process then uses it as any SHM
 * (semaphores, counter, keywords). Nothing is sent for an SHM nobody uses; a
 * replica nobody maps any more is dropped. A write to a replica goes to the
 * machine owning the SHM, and comes back to every replica.
 *
 * This header gives the client call libdao and the language bindings use
 * (daoNetResolve), and the wire protocol, for other implementations.
 * See docs/source/shm_net.rst.
 */
#ifndef DAO_NET_H
#define DAO_NET_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define DAO_NET_EXPORT __declspec(dllexport)
#else
#define DAO_NET_EXPORT
#endif

/* ------------------------------------------------------------------------- */
/* Defaults (each can be changed: daoShmNetd options, or the environment)    */
/* ------------------------------------------------------------------------- */

#define DAO_NET_DATA_PORT      7710            /**< TCP: catalogs and SHM streams between services  */
#define DAO_NET_CONTROL_PORT   7709            /**< TCP on 127.0.0.1 only: local commands          */
#define DAO_NET_BEACON_PORT    7711            /**< UDP: discovery                                   */
#define DAO_NET_BEACON_GROUP   "239.255.77.10" /**< UDP multicast group of the discovery            */
#define DAO_NET_SEM            9               /**< the semaphore of each SHM the service waits on  */
#define DAO_NET_RESOLVE_MS     5000            /**< how long an open waits for a remote SHM         */

/* environment */
#define DAO_NET_ENV_ENABLE     "DAO_NET"            /**< "0": libdao never asks the service      */
#define DAO_NET_ENV_CONTROL    "DAO_NET_CONTROL"    /**< the local service's control port        */
#define DAO_NET_ENV_TIMEOUT    "DAO_NET_TIMEOUT_MS" /**< how long an open waits (ms)             */

/* ------------------------------------------------------------------------- */
/* Client                                                                     */
/* ------------------------------------------------------------------------- */

/**
 * @brief A remote SHM, made local: ask the local daoShmNetd for `name` and return the
 * path of its local replica, once it holds the SHM's data.
 *
 * @param name   an SHM path ("/tmp/dm1Cmd.im.shm"), a bare name ("dm1Cmd"), or either
 *               prefixed by the machine that owns it ("rtc1:/tmp/dm1Cmd.im.shm",
 *               "rtc1:dm1Cmd") when several machines have that name.
 * @param path   receives the local path to open.
 * @param len    size of `path`.
 * @param timeout_ms  how long to wait (<= 0: DAO_NET_TIMEOUT_MS, else DAO_NET_RESOLVE_MS).
 * @return 0 (DAO_SUCCESS) with `path` set; 1 (DAO_ERROR) if no service runs, the SHM is
 *         nowhere, or it did not come in time -- quickly when no service runs.
 *
 * libdao's daoShmOpen calls it when the file does not exist (unless DAO_NET=0).
 */
DAO_NET_EXPORT int daoNetResolve(const char *name, char *path, size_t len, int timeout_ms);

/* ------------------------------------------------------------------------- */
/* Wire protocol (version 1). Integers little-endian.                          */
/*                                                                             */
/* Discovery: a UDP datagram, one text line, every second:                     */
/*   DAONET 1 domain=<domain> node=<node> port=<data port> catalog=<version>   */
/*                                                                             */
/* Data port, TCP: the client sends one text line, then                         */
/*   "CATALOG\n"                -> lines "SHM <name> <naxis> <s0> <s1> <s2> <atype>  */
/*                                 <nbkw> <fifo> <gpu>\n", then "END\n"; closed    */
/*   "SUBSCRIBE <name> <node>\n"-> "OK\n" (or "ERR <why>\n"), then binary messages  */
/*                                 both ways: DAO_NET_META, then DAO_NET_FRAMEs  */
/*                                 down; DAO_NET_WRITEs up; HEARTBEATs both ways   */
/* Every binary message is a DaoNetHeader followed by `size` bytes of payload;   */
/* a FRAME is also followed by a uint64 trailer: the SHM counter after the copy. */
/* ------------------------------------------------------------------------- */

#define DAO_NET_MAGIC          0x54454E44u   /**< 'DNET' */
#define DAO_NET_VERSION        1u

enum {
    DAO_NET_META      = 1,   /**< payload: DaoNetMeta, then nbkw DaoNetKeyword          */
    DAO_NET_FRAME     = 2,   /**< payload: the frame's bytes; then the uint64 trailer   */
    DAO_NET_HEARTBEAT = 3,   /**< no payload: the link is alive                          */
    DAO_NET_WRITE     = 4,   /**< payload: the frame's bytes, to write into the source   */
    DAO_NET_BYE       = 5    /**< no payload: the sender closes                          */
};

#define DAO_NET_FLAG_ECHO  1u   /**< FRAME: the owner writing back this subscriber's own WRITE */

typedef struct {
    uint32_t magic;       /**< DAO_NET_MAGIC                                           */
    uint16_t version;     /**< DAO_NET_VERSION                                         */
    uint16_t type;        /**< DAO_NET_META, ...                                       */
    uint32_t flags;       /**< DAO_NET_FLAG_...                                        */
    uint32_t dropped;     /**< FRAME: frames the sender skipped so far (latest only)   */
    uint64_t cnt0;        /**< FRAME: the SHM counter of the frame                     */
    uint64_t seq;         /**< messages sent on this stream                            */
    int64_t  atime_ns;    /**< FRAME: the frame's write time on the source (UTC ns)    */
    uint64_t size;        /**< payload bytes following                                 */
    uint64_t reserved[2];
} DaoNetHeader;           /* 64 bytes */

typedef struct {
    char     name[256];   /**< the SHM's name ("dm1Cmd")                               */
    uint8_t  naxis;
    uint8_t  atype;       /**< dao data type (_DATATYPE_...)                           */
    uint16_t nbkw;        /**< keywords following                                      */
    uint32_t size[3];
    uint64_t nelement;
    uint64_t elsize;      /**< bytes per element                                       */
} DaoNetMeta;             /* 288 bytes */

typedef struct {
    char    name[16];
    char    type;         /**< 'N', 'L', 'D', 'S'                                      */
    uint8_t pad[7];
    uint8_t value[16];    /**< int64 / double / 16 characters, as IMAGE_KEYWORD      */
    char    comment[80];
} DaoNetKeyword;          /* 120 bytes */

#ifdef __cplusplus
}
#endif

#endif /* DAO_NET_H */
