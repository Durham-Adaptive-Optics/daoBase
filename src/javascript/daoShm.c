/*
 * daoShm.c - Node.js binding of the daoBase SHM API (dao.h), as a Node-API addon.
 *
 * Plain Node-API C (node_api.h): no npm dependency, and the IMAGE / IMAGE_METADATA
 * layout comes from dao.h itself, so it cannot drift out of sync with libdao.
 *
 * Exposes one class, Shm (wrapped by index.js):
 *   new Shm(name)                         open an existing SHM
 *   new Shm(name, shape, atype, depth)    create (or overwrite) one
 *   getData()          copy of the newest frame, as a TypedArray
 *   setData(typed)     write a frame (posts the semaphores, bumps the counter)
 *   wait(ms, cb)       wait for the next frame on this handle's own semaphore, off
 *                      the event loop; cb(err, posted) (false: timed out)
 *   close()            close (deferred until a pending wait returns)
 *   counter, shape, atype, depth, name, closed   read-only properties
 */
#define NAPI_VERSION 8
#include <node_api.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "dao.h"

typedef struct {
    IMAGE img;
    int open;               /* daoShmOpen/Create succeeded and not closed yet */
    int closing;            /* close() called while a wait was pending */
    int waiting;            /* a wait is running on a worker thread */
    /* wait state */
    napi_async_work work;
    napi_ref self;          /* keeps the JS object alive while waiting */
    napi_ref cb;
    int64_t timeout_ms;
    int result;             /* daoShmWaitSemTimeout's */
} Shm;

#define CHECK(call)                                                            \
    do {                                                                       \
        if ((call) != napi_ok) {                                               \
            const napi_extended_error_info* info_;                             \
            napi_get_last_error_info(env, &info_);                             \
            napi_throw_error(env, NULL, info_ && info_->error_message          \
                             ? info_->error_message : #call " failed");        \
            return NULL;                                                       \
        }                                                                      \
    } while (0)

#define THROW(...)                                                             \
    do {                                                                       \
        char msg_[512];                                                        \
        snprintf(msg_, sizeof msg_, __VA_ARGS__);                              \
        napi_throw_error(env, NULL, msg_);                                     \
        return NULL;                                                           \
    } while (0)

/* bytes per element, and TypedArray type, of each dao atype (1..12) */
static size_t elem_size(uint8_t atype)
{
    static const size_t sz[] = {0, 1, 1, 2, 2, 4, 4, 8, 8, 4, 8, 8, 16};
    return atype < sizeof sz / sizeof sz[0] ? sz[atype] : 0;
}

static int typed_type(uint8_t atype, napi_typedarray_type* t)
{
    switch (atype) {
    case _DATATYPE_UINT8:          *t = napi_uint8_array;     return 1;
    case _DATATYPE_INT8:           *t = napi_int8_array;      return 1;
    case _DATATYPE_UINT16:         *t = napi_uint16_array;    return 1;
    case _DATATYPE_INT16:          *t = napi_int16_array;     return 1;
    case _DATATYPE_UINT32:         *t = napi_uint32_array;    return 1;
    case _DATATYPE_INT32:          *t = napi_int32_array;     return 1;
    case _DATATYPE_UINT64:         *t = napi_biguint64_array; return 1;
    case _DATATYPE_INT64:          *t = napi_bigint64_array;  return 1;
    case _DATATYPE_FLOAT:          *t = napi_float32_array;   return 1;
    case _DATATYPE_DOUBLE:         *t = napi_float64_array;   return 1;
    case _DATATYPE_COMPLEX_FLOAT:  *t = napi_float32_array;   return 1;  /* re, im interleaved */
    case _DATATYPE_COMPLEX_DOUBLE: *t = napi_float64_array;   return 1;
    default: return 0;
    }
}

static void shm_close(Shm* s)
{
    if (s->open) {
        daoShmClose(&s->img);   /* also gives the claimed semaphore back */
        s->open = 0;
    }
}

static void shm_finalize(napi_env env, void* data, void* hint)
{
    (void)env; (void)hint;
    Shm* s = data;
    shm_close(s);   /* never while waiting: the wait holds a reference */
    free(s);
}

static Shm* unwrap(napi_env env, napi_callback_info info, size_t* argc, napi_value* argv,
                   int need_open)
{
    napi_value self;
    Shm* s = NULL;
    if (napi_get_cb_info(env, info, argc, argv, &self, NULL) != napi_ok
        || napi_unwrap(env, self, (void**)&s) != napi_ok || !s) {
        napi_throw_error(env, NULL, "dao: not a Shm");
        return NULL;
    }
    if (need_open && (!s->open || s->closing)) {
        napi_throw_error(env, NULL, "dao: SHM closed");
        return NULL;
    }
    return s;
}

/* daoShmOpen exit()s the whole process on a SHM whose first or second axis is 0 (a
 * 1-D SHM). Read its metadata first, and refuse those with a JS error instead.
 * Returns 0 if daoShmOpen can be called. */
static int precheck_open(const char* name, char* why, size_t whylen)
{
    IMAGE_METADATA md;
    int fd = open(name, O_RDONLY);
    if (fd < 0)
        return 0;   /* let daoShmOpen report it */
    ssize_t n = pread(fd, &md, sizeof md, 0);
    close(fd);
    if (n != (ssize_t)sizeof md || md.magic != DAO_SHM_MAGIC || md.layout != DAO_SHM_LAYOUT_VERSION)
        return 0;   /* daoShmOpen refuses these with an error, without exiting */
    if (md.size[0] < 1 || md.size[1] < 1) {
        snprintf(why, whylen, "dao: SHM %s has naxis %u, size %u x %u: libdao cannot open a "
                 "SHM with an axis of size 0 (1-D SHMs: make them [n, 1])",
                 name, md.naxis, md.size[0], md.size[1]);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ constructor */

static napi_value Shm_new(napi_env env, napi_callback_info info)
{
    size_t argc = 4;
    napi_value argv[4], self, target;
    CHECK(napi_get_cb_info(env, info, &argc, argv, &self, NULL));
    CHECK(napi_get_new_target(env, info, &target));
    if (!target)
        THROW("dao: Shm is a constructor, use new");
    if (argc < 1)
        THROW("dao: Shm(name[, shape, atype, depth])");

    char name[DAO_SHM_NAME_LEN];
    size_t len;
    if (napi_get_value_string_utf8(env, argv[0], name, sizeof name, &len) != napi_ok)
        THROW("dao: the SHM name must be a string");
    if (len >= sizeof name - 1)
        THROW("dao: SHM name too long (%d bytes max)", DAO_SHM_NAME_LEN - 1);

    Shm* s = calloc(1, sizeof *s);
    if (!s)
        THROW("dao: out of memory");

    napi_valuetype t = napi_undefined;
    if (argc >= 2)
        napi_typeof(env, argv[1], &t);

    if (t == napi_undefined || t == napi_null) {
        char why[DAO_SHM_NAME_LEN + 160];
        if (precheck_open(name, why, sizeof why) != 0) {
            free(s);
            napi_throw_error(env, NULL, why);
            return NULL;
        }
        if (daoShmOpen(name, &s->img) != DAO_SUCCESS) {
            free(s);
            THROW("dao: cannot open SHM %s", name);
        }
    } else {
        /* create: shape (array of 1-3 sizes, same order as Python's numpy shape) */
        bool isArray = false;
        uint32_t naxis = 0, size[3];
        napi_is_array(env, argv[1], &isArray);
        if (isArray)
            napi_get_array_length(env, argv[1], &naxis);
        if (!isArray || naxis < 1 || naxis > 3) {
            free(s);
            THROW("dao: shape must be an array of 1 to 3 sizes");
        }
        for (uint32_t i = 0; i < naxis; i++) {
            napi_value v;
            napi_get_element(env, argv[1], i, &v);
            if (napi_get_value_uint32(env, v, &size[i]) != napi_ok || size[i] == 0) {
                free(s);
                THROW("dao: shape sizes must be positive integers");
            }
        }
        uint32_t atype = 0, depth = 1;
        if (argc >= 3)
            napi_get_value_uint32(env, argv[2], &atype);
        if (argc >= 4)
            napi_get_value_uint32(env, argv[3], &depth);
        if (elem_size((uint8_t)atype) == 0) {
            free(s);
            THROW("dao: unknown atype %u", atype);
        }
        if (depth < 1)
            depth = 1;
        if (daoShmCreateFifo(&s->img, name, naxis, size, (uint8_t)atype, 1, 0, depth)
            != DAO_SUCCESS) {
            free(s);
            THROW("dao: cannot create SHM %s", name);
        }
    }
    s->open = 1;
    if (napi_wrap(env, self, s, shm_finalize, NULL, NULL) != napi_ok) {
        shm_close(s);
        free(s);
        THROW("dao: cannot wrap the SHM");
    }
    return self;
}

/* ------------------------------------------------------------------ data */

static napi_value Shm_getData(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    Shm* s = unwrap(env, info, &argc, NULL, 1);
    if (!s)
        return NULL;
    IMAGE_METADATA* md = s->img.md;
    napi_typedarray_type tt;
    if (!typed_type(md->atype, &tt))
        THROW("dao: unsupported atype %u", md->atype);

    void* seg = NULL;
    uint32_t idx = 0;
    uint64_t cnt0 = 0;
    if (daoShmGetData(&s->img, &seg, &idx, &cnt0) != DAO_SUCCESS || !seg)
        THROW("dao: cannot read SHM %s", md->name);

    size_t bytes = md->nelement * elem_size(md->atype);
    size_t length = md->nelement;
    if (md->atype == _DATATYPE_COMPLEX_FLOAT || md->atype == _DATATYPE_COMPLEX_DOUBLE)
        length *= 2;

    void* dst;
    napi_value ab, out;
    CHECK(napi_create_arraybuffer(env, bytes, &dst, &ab));
    memcpy(dst, seg, bytes);   /* a copy: stays valid when the writer recreates the SHM */
    CHECK(napi_create_typedarray(env, tt, length, ab, 0, &out));
    return out;
}

static napi_value Shm_setData(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1];
    Shm* s = unwrap(env, info, &argc, argv, 1);
    if (!s)
        return NULL;
    IMAGE_METADATA* md = s->img.md;

    bool isTyped = false;
    if (argc < 1 || napi_is_typedarray(env, argv[0], &isTyped) != napi_ok || !isTyped)
        THROW("dao: setData needs a TypedArray");
    napi_typedarray_type tt, want;
    size_t length, offset;
    void* data;
    napi_value ab;
    CHECK(napi_get_typedarray_info(env, argv[0], &tt, &length, &data, &ab, &offset));
    if (!typed_type(md->atype, &want))
        THROW("dao: unsupported atype %u", md->atype);
    size_t expected = md->nelement;
    if (md->atype == _DATATYPE_COMPLEX_FLOAT || md->atype == _DATATYPE_COMPLEX_DOUBLE)
        expected *= 2;
    if (tt != want)
        THROW("dao: setData: wrong TypedArray type for SHM %s (atype %u)", md->name, md->atype);
    if (length != expected)
        THROW("dao: setData: %zu values given, SHM %s holds %zu", length, md->name, expected);

    if (daoShmSetData(&s->img, data, (uint32_t)md->nelement) != DAO_SUCCESS)
        THROW("dao: cannot write SHM %s", md->name);
    return NULL;
}

/* ------------------------------------------------------------------ wait */

static void wait_execute(napi_env env, void* data)
{
    (void)env;
    Shm* s = data;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);   /* absolute, as sem_timedwait wants */
    ts.tv_sec += s->timeout_ms / 1000;
    ts.tv_nsec += (s->timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    s->result = daoShmWaitSemTimeout(&s->img, DAO_SEM_AUTO, &ts);
}

static void wait_complete(napi_env env, napi_status status, void* data)
{
    Shm* s = data;
    napi_value cb, self, global, argv[2];
    napi_get_reference_value(env, s->cb, &cb);
    napi_get_reference_value(env, s->self, &self);
    napi_get_global(env, &global);

    if (status == napi_cancelled) {
        napi_create_string_utf8(env, "dao: wait cancelled", NAPI_AUTO_LENGTH, &argv[1]);
        napi_create_error(env, NULL, argv[1], &argv[0]);
        napi_get_undefined(env, &argv[1]);
    } else if (s->result == DAO_SUCCESS || s->result == DAO_TIMEOUT) {
        napi_get_null(env, &argv[0]);
        napi_get_boolean(env, s->result == DAO_SUCCESS, &argv[1]);
    } else {
        napi_create_string_utf8(env, "dao: cannot wait on the SHM semaphore",
                                NAPI_AUTO_LENGTH, &argv[1]);
        napi_create_error(env, NULL, argv[1], &argv[0]);
        napi_get_undefined(env, &argv[1]);
    }

    s->waiting = 0;
    napi_delete_async_work(env, s->work);
    if (s->closing) {
        shm_close(s);
        s->closing = 0;
    }
    napi_delete_reference(env, s->cb);
    s->cb = NULL;

    napi_value ignored;
    napi_call_function(env, global, cb, 2, argv, &ignored);   /* may throw: reported by Node */
    napi_delete_reference(env, s->self);   /* last: the object may be collected after this */
    s->self = NULL;
}

static napi_value Shm_wait(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value argv[2], self;
    Shm* s = NULL;
    CHECK(napi_get_cb_info(env, info, &argc, argv, &self, NULL));
    CHECK(napi_unwrap(env, self, (void**)&s));
    if (!s->open || s->closing)
        THROW("dao: SHM closed");
    if (s->waiting)
        THROW("dao: a wait is already pending on this Shm (one per handle: open another)");

    napi_valuetype t;
    double ms = 0;
    if (argc < 2 || napi_get_value_double(env, argv[0], &ms) != napi_ok
        || napi_typeof(env, argv[1], &t) != napi_ok || t != napi_function)
        THROW("dao: wait(timeoutMs, callback)");
    if (!(ms > 0))
        THROW("dao: wait needs a positive timeout (ms): a wait without one holds a worker thread");

    napi_value resource;
    CHECK(napi_create_string_utf8(env, "daoShmWait", NAPI_AUTO_LENGTH, &resource));
    CHECK(napi_create_reference(env, self, 1, &s->self));
    CHECK(napi_create_reference(env, argv[1], 1, &s->cb));
    s->timeout_ms = (int64_t)ms;
    if (s->timeout_ms < 1)
        s->timeout_ms = 1;
    CHECK(napi_create_async_work(env, NULL, resource, wait_execute, wait_complete, s, &s->work));
    CHECK(napi_queue_async_work(env, s->work));
    s->waiting = 1;
    return NULL;
}

/* ------------------------------------------------------------------ close, properties */

static napi_value Shm_close(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    Shm* s = unwrap(env, info, &argc, NULL, 0);
    if (!s)
        return NULL;
    if (s->waiting)
        s->closing = 1;   /* the worker uses the IMAGE: close when the wait returns */
    else
        shm_close(s);
    return NULL;
}

static napi_value Shm_counter(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    Shm* s = unwrap(env, info, &argc, NULL, 1);
    if (!s)
        return NULL;
    napi_value v;
    CHECK(napi_create_bigint_uint64(env, daoShmGetCounter(&s->img), &v));
    return v;
}

static napi_value Shm_shape(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    Shm* s = unwrap(env, info, &argc, NULL, 1);
    if (!s)
        return NULL;
    napi_value arr, v;
    CHECK(napi_create_array_with_length(env, s->img.md->naxis, &arr));
    for (uint32_t i = 0; i < s->img.md->naxis; i++) {
        CHECK(napi_create_uint32(env, s->img.md->size[i], &v));
        CHECK(napi_set_element(env, arr, i, v));
    }
    return arr;
}

static napi_value Shm_atype(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    Shm* s = unwrap(env, info, &argc, NULL, 1);
    if (!s)
        return NULL;
    napi_value v;
    CHECK(napi_create_uint32(env, s->img.md->atype, &v));
    return v;
}

static napi_value Shm_depth(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    Shm* s = unwrap(env, info, &argc, NULL, 1);
    if (!s)
        return NULL;
    napi_value v;
    CHECK(napi_create_uint32(env, s->img.md->fifo_size ? s->img.md->fifo_size : 1, &v));
    return v;
}

static napi_value Shm_name(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    Shm* s = unwrap(env, info, &argc, NULL, 0);
    if (!s)
        return NULL;
    napi_value v;
    CHECK(napi_create_string_utf8(env, s->img.name, NAPI_AUTO_LENGTH, &v));
    return v;
}

static napi_value Shm_closed(napi_env env, napi_callback_info info)
{
    size_t argc = 0;
    Shm* s = unwrap(env, info, &argc, NULL, 0);
    if (!s)
        return NULL;
    napi_value v;
    CHECK(napi_get_boolean(env, !s->open || s->closing, &v));
    return v;
}

static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor props[] = {
        {"getData", NULL, Shm_getData, NULL, NULL, NULL, napi_default, NULL},
        {"setData", NULL, Shm_setData, NULL, NULL, NULL, napi_default, NULL},
        {"wait",    NULL, Shm_wait,    NULL, NULL, NULL, napi_default, NULL},
        {"close",   NULL, Shm_close,   NULL, NULL, NULL, napi_default, NULL},
        {"counter", NULL, NULL, Shm_counter, NULL, NULL, napi_default, NULL},
        {"shape",   NULL, NULL, Shm_shape,   NULL, NULL, napi_default, NULL},
        {"atype",   NULL, NULL, Shm_atype,   NULL, NULL, napi_default, NULL},
        {"depth",   NULL, NULL, Shm_depth,   NULL, NULL, napi_default, NULL},
        {"name",    NULL, NULL, Shm_name,    NULL, NULL, napi_default, NULL},
        {"closed",  NULL, NULL, Shm_closed,  NULL, NULL, napi_default, NULL},
    };
    napi_value cls;
    CHECK(napi_define_class(env, "Shm", NAPI_AUTO_LENGTH, Shm_new, NULL,
                            sizeof props / sizeof props[0], props, &cls));
    CHECK(napi_set_named_property(env, exports, "Shm", cls));
    return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
