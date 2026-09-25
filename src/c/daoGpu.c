/**
 * @file    daoGpu.c
 * @brief   GPU SHMs: metadata and semaphores in the /tmp file, payload on a GPU.
 *
 * The payload is a CUDA VMM allocation (cuMemCreate) exported as a POSIX file
 * descriptor. The descriptor is handed to the daoGpuShmd daemon, which keeps
 * the allocation alive independently of the processes using it; daoShmOpen
 * gets the descriptor from the daemon and maps the allocation into the
 * calling process (image->d_array).
 *
 * The CUDA driver (libcuda.so.1) is loaded with dlopen on first use, so libdao
 * does not depend on it: machines without a GPU run CPU SHMs as before. The
 * allocations live in the primary context of their device, the one the CUDA
 * runtime uses, so image->d_array can be passed to kernels directly.
 *
 * Built only with DAO_HAVE_CUDA on Linux; otherwise the functions report that
 * GPU SHMs are not available.
 */
#define _GNU_SOURCE
#include "dao.h"
#include "daoGpuInternal.h"

#if defined(__linux__) && defined(DAO_HAVE_CUDA)

#include <cuda.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include "daoGpuProto.h"

/* ---------------------------------------------------------------------------
 * CUDA driver API, resolved at run time
 * ------------------------------------------------------------------------- */
static struct {
    CUresult (*Init)(unsigned int);
    CUresult (*DeviceGetCount)(int *);
    CUresult (*DeviceGet)(CUdevice *, int);
    CUresult (*DeviceGetUuid)(CUuuid *, CUdevice);
    CUresult (*DeviceGetAttribute)(int *, CUdevice_attribute, CUdevice);
    CUresult (*DevicePrimaryCtxRetain)(CUcontext *, CUdevice);
    CUresult (*CtxPushCurrent)(CUcontext);
    CUresult (*CtxPopCurrent)(CUcontext *);
    CUresult (*MemGetAllocationGranularity)(size_t *, const CUmemAllocationProp *,
                                            CUmemAllocationGranularity_flags);
    CUresult (*MemCreate)(CUmemGenericAllocationHandle *, size_t, const CUmemAllocationProp *,
                          unsigned long long);
    CUresult (*MemRelease)(CUmemGenericAllocationHandle);
    CUresult (*MemExportToShareableHandle)(void *, CUmemGenericAllocationHandle,
                                           CUmemAllocationHandleType, unsigned long long);
    CUresult (*MemImportFromShareableHandle)(CUmemGenericAllocationHandle *, void *,
                                             CUmemAllocationHandleType);
    CUresult (*MemAddressReserve)(CUdeviceptr *, size_t, size_t, CUdeviceptr, unsigned long long);
    CUresult (*MemAddressFree)(CUdeviceptr, size_t);
    CUresult (*MemMap)(CUdeviceptr, size_t, size_t, CUmemGenericAllocationHandle, unsigned long long);
    CUresult (*MemUnmap)(CUdeviceptr, size_t);
    CUresult (*MemSetAccess)(CUdeviceptr, size_t, const CUmemAccessDesc *, size_t);
    CUresult (*MemcpyHtoD)(CUdeviceptr, const void *, size_t);
    CUresult (*MemcpyDtoH)(void *, CUdeviceptr, size_t);
    CUresult (*MemcpyDtoHAsync)(void *, CUdeviceptr, size_t, CUstream);
    CUresult (*MemcpyDtoDAsync)(CUdeviceptr, CUdeviceptr, size_t, CUstream);
    CUresult (*MemsetD8)(CUdeviceptr, unsigned char, size_t);
    CUresult (*StreamSynchronize)(CUstream);
    CUresult (*LaunchHostFunc)(CUstream, CUhostFn, void *);
    CUresult (*MemHostRegister)(void *, size_t, unsigned int);
    CUresult (*MemHostUnregister)(void *);
    CUresult (*GetErrorString)(CUresult, const char **);
} cu;

static pthread_once_t load_once = PTHREAD_ONCE_INIT;
static int  load_ok = 0;
static char load_error[256] = "";

static void load_driver(void)
{
    void *h = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        snprintf(load_error, sizeof load_error, "cannot load the CUDA driver: %s", dlerror());
        return;
    }
#define SYM(field, name)                                                          \
    if (!(*(void **) &cu.field = dlsym(h, name))) {                               \
        snprintf(load_error, sizeof load_error, "CUDA driver lacks %s", name);   \
        return;                                                                   \
    }
    SYM(Init, "cuInit");
    SYM(DeviceGetCount, "cuDeviceGetCount");
    SYM(DeviceGet, "cuDeviceGet");
    SYM(DeviceGetUuid, "cuDeviceGetUuid");
    SYM(DeviceGetAttribute, "cuDeviceGetAttribute");
    SYM(DevicePrimaryCtxRetain, "cuDevicePrimaryCtxRetain");
    SYM(CtxPushCurrent, "cuCtxPushCurrent_v2");
    SYM(CtxPopCurrent, "cuCtxPopCurrent_v2");
    SYM(MemGetAllocationGranularity, "cuMemGetAllocationGranularity");
    SYM(MemCreate, "cuMemCreate");
    SYM(MemRelease, "cuMemRelease");
    SYM(MemExportToShareableHandle, "cuMemExportToShareableHandle");
    SYM(MemImportFromShareableHandle, "cuMemImportFromShareableHandle");
    SYM(MemAddressReserve, "cuMemAddressReserve");
    SYM(MemAddressFree, "cuMemAddressFree");
    SYM(MemMap, "cuMemMap");
    SYM(MemUnmap, "cuMemUnmap");
    SYM(MemSetAccess, "cuMemSetAccess");
    SYM(MemcpyHtoD, "cuMemcpyHtoD_v2");
    SYM(MemcpyDtoH, "cuMemcpyDtoH_v2");
    SYM(MemcpyDtoHAsync, "cuMemcpyDtoHAsync_v2");
    SYM(MemcpyDtoDAsync, "cuMemcpyDtoDAsync_v2");
    SYM(MemsetD8, "cuMemsetD8_v2");
    SYM(StreamSynchronize, "cuStreamSynchronize");
    SYM(LaunchHostFunc, "cuLaunchHostFunc");
    SYM(MemHostRegister, "cuMemHostRegister_v2");
    SYM(MemHostUnregister, "cuMemHostUnregister");
    SYM(GetErrorString, "cuGetErrorString");
#undef SYM
    if (cu.Init(0) != CUDA_SUCCESS) {
        snprintf(load_error, sizeof load_error, "cuInit failed (no usable GPU / driver)");
        return;
    }
    load_ok = 1;
}

static int driver_ok(void)
{
    pthread_once(&load_once, load_driver);
    return load_ok;
}

static const char *cuerr(CUresult r)
{
    const char *s = NULL;
    if (cu.GetErrorString)
        cu.GetErrorString(r, &s);
    return s ? s : "unknown CUDA error";
}

#define CU_TRY(call)                                                  \
    do {                                                              \
        CUresult r_ = (call);                                         \
        if (r_ != CUDA_SUCCESS) {                                     \
            daoError("%s: %s\n", #call, cuerr(r_));                   \
            goto fail;                                                \
        }                                                             \
    } while (0)

/* ---------------------------------------------------------------------------
 * per-image state
 * ------------------------------------------------------------------------- */
typedef struct {
    CUdevice  dev;
    CUcontext ctx;
    CUmemGenericAllocationHandle handle;
    CUdeviceptr dptr;
    size_t    size;            /* mapped bytes (granularity rounded) */
    size_t    bytes;           /* payload bytes                     */
    void     *host;            /* /tmp host copy (image->array)     */
    void     *reg_base;        /* page-aligned registered range     */
    size_t    reg_len;
    int       mirror;
} GpuState;

static size_t elem_size(uint8_t atype)
{
    switch (atype) {
    case _DATATYPE_UINT8:  case _DATATYPE_INT8:  return 1;
    case _DATATYPE_UINT16: case _DATATYPE_INT16: return 2;
    case _DATATYPE_UINT32: case _DATATYPE_INT32: case _DATATYPE_FLOAT: return 4;
    case _DATATYPE_UINT64: case _DATATYPE_INT64: case _DATATYPE_DOUBLE:
    case _DATATYPE_COMPLEX_FLOAT: return 8;
    case _DATATYPE_COMPLEX_DOUBLE: return 16;
    default: return 0;
    }
}

static int push(GpuState *st)
{
    CUresult r = cu.CtxPushCurrent(st->ctx);
    if (r != CUDA_SUCCESS)
        daoError("cuCtxPushCurrent: %s\n", cuerr(r));
    return r == CUDA_SUCCESS;
}

static void pop(void)
{
    CUcontext c;
    cu.CtxPopCurrent(&c);
}

static CUmemAllocationProp alloc_prop(CUdevice dev)
{
    CUmemAllocationProp p;
    memset(&p, 0, sizeof p);
    p.type = CU_MEM_ALLOCATION_TYPE_PINNED;
    p.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    p.location.id = (int) dev;
    p.requestedHandleTypes = CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR;
    return p;
}

/* Reserve a virtual range, map st->handle into it, allow read/write. */
static int map_handle(GpuState *st)
{
    CUmemAccessDesc acc;
    CU_TRY(cu.MemAddressReserve(&st->dptr, st->size, 0, 0, 0));
    CU_TRY(cu.MemMap(st->dptr, st->size, 0, st->handle, 0));
    memset(&acc, 0, sizeof acc);
    acc.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
    acc.location.id = (int) st->dev;
    acc.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
    CU_TRY(cu.MemSetAccess(st->dptr, st->size, &acc, 1));
    return 1;
fail:
    return 0;
}

/* Pin the /tmp host copy so that commits can copy it asynchronously. */
static void register_host(GpuState *st)
{
    long page = sysconf(_SC_PAGESIZE);
    uintptr_t start = (uintptr_t) st->host & ~(uintptr_t) (page - 1);
    uintptr_t end = ((uintptr_t) st->host + st->bytes + page - 1) & ~(uintptr_t) (page - 1);
    if (cu.MemHostRegister((void *) start, end - start, CU_MEMHOSTREGISTER_PORTABLE) == CUDA_SUCCESS) {
        st->reg_base = (void *) start;
        st->reg_len = end - start;
    } else {
        daoDebug("host copy could not be pinned; mirrored commits copy synchronously\n");
    }
}

static void release_state(GpuState *st)
{
    if (!st)
        return;
    if (st->reg_base)
        cu.MemHostUnregister(st->reg_base);
    if (st->dptr) {
        cu.MemUnmap(st->dptr, st->size);
        cu.MemAddressFree(st->dptr, st->size);
    }
    if (st->handle)
        cu.MemRelease(st->handle);
    free(st);
}

static GpuState *state_of(IMAGE *image)
{
    return image ? (GpuState *) image->gpu : NULL;
}

/* ---------------------------------------------------------------------------
 * daoGpuShmd daemon
 * ------------------------------------------------------------------------- */
static void start_daemon(void)
{
    char candidates[4][4096];
    int n = 0, i;
    const char *env = getenv("DAO_GPU_DAEMON");
    const char *root = getenv("DAOROOT");
    const char *path = getenv("PATH");
    char log_path[256];
    char *argv_[] = { (char *) "daoGpuShmd", NULL };
    pid_t pid;

    /* resolve everything before fork(): only async-signal-safe calls after it */
    if (env && *env)
        snprintf(candidates[n++], sizeof candidates[0], "%s", env);
    if (root && *root)
        snprintf(candidates[n++], sizeof candidates[0], "%s/bin/daoGpuShmd", root);
    if (path) {
        const char *p = path;
        while (*p && n < 4) {
            const char *e = strchr(p, ':');
            size_t len = e ? (size_t) (e - p) : strlen(p);
            char try_[4096];
            snprintf(try_, sizeof try_, "%.*s/daoGpuShmd", (int) len, p);
            if (access(try_, X_OK) == 0) {
                snprintf(candidates[n++], sizeof candidates[0], "%s", try_);
                break;
            }
            p = e ? e + 1 : p + len;
        }
    }
    snprintf(log_path, sizeof log_path, "/tmp/daoGpuShmd-%u.log", (unsigned) getuid());

    pid = fork();
    if (pid == 0) {
        int fd, nul;
        setsid();
        if (fork() != 0)
            _exit(0);
        for (fd = 3; fd < 4096; fd++)        /* do not leak SHM / driver descriptors */
            close(fd);
        nul = open("/dev/null", O_RDONLY);
        fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (nul >= 0) {
            dup2(nul, 0);
            if (nul > 2)
                close(nul);
        }
        if (fd >= 0) {
            dup2(fd, 1);
            dup2(fd, 2);
            if (fd > 2)
                close(fd);
        }
        for (i = 0; i < n; i++)
            execv(candidates[i], argv_);
        _exit(127);
    }
    if (pid > 0)
        waitpid(pid, NULL, 0);
}

/* One request/reply with the daemon; starts it when autostart is set. */
static int daemon_call(daoGpuMsg *req, int fd_out, daoGpuMsg *rep, int *fd_in, int autostart)
{
    int s = daoGpuConnect(), tries;
    if (s < 0 && autostart) {
        start_daemon();
        for (tries = 0; tries < 100 && s < 0; tries++) {
            struct timespec ts = { 0, 20 * 1000 * 1000 };
            nanosleep(&ts, NULL);
            s = daoGpuConnect();
        }
    }
    if (s < 0) {
        daoError("cannot reach daoGpuShmd (is it installed in $DAOROOT/bin or on PATH?)\n");
        return 0;
    }
    if (daoGpuSend(s, req, fd_out) != 0 || daoGpuRecv(s, rep, fd_in) != 0) {
        daoError("no reply from daoGpuShmd\n");
        close(s);
        return 0;
    }
    close(s);
    return 1;
}

static uint64_t new_gpu_id(void)
{
    static uint64_t counter = 0;
    struct timespec ts;
    uint64_t id;
    clock_gettime(CLOCK_REALTIME, &ts);
    id = ((uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec)
         ^ ((uint64_t) getpid() << 40) ^ __atomic_add_fetch(&counter, 1, __ATOMIC_RELAXED);
    return id ? id : 1;
}

static int device_by_uuid(const uint8_t uuid[16], CUdevice *out)
{
    int count = 0, i;
    cu.DeviceGetCount(&count);
    for (i = 0; i < count; i++) {
        CUdevice d;
        CUuuid u;
        if (cu.DeviceGet(&d, i) == CUDA_SUCCESS && cu.DeviceGetUuid(&u, d) == CUDA_SUCCESS
            && memcmp(u.bytes, uuid, 16) == 0) {
            *out = d;
            return 1;
        }
    }
    return 0;
}

/* Is an NVIDIA MPS control daemon running for this pipe directory? Its pid
 * file disappears when it stops (the control socket does not). */
static int mps_running(void)
{
    const char *dir = getenv("CUDA_MPS_PIPE_DIRECTORY");
    char path[512];
    long pid = 0;
    FILE *f;
    snprintf(path, sizeof path, "%s/nvidia-cuda-mps-control.pid", dir && *dir ? dir : "/tmp/nvidia-mps");
    if (!(f = fopen(path, "r")))
        return 0;
    if (fscanf(f, "%ld", &pid) != 1)
        pid = 0;
    fclose(f);
    return pid > 0 && (kill((pid_t) pid, 0) == 0 || errno == EPERM);
}

/* Once per process: GPU SHM pipelines need MPS to be fast. */
static void warn_if_no_mps(void)
{
    static int done = 0;
    const char *quiet = getenv("DAO_GPU_NO_MPS_WARNING");
    if (__atomic_exchange_n(&done, 1, __ATOMIC_RELAXED))
        return;
    if (quiet && *quiet && strcmp(quiet, "0") != 0)
        return;
    if (!mps_running())
        daoWarning("NVIDIA MPS is not running: passing GPU SHMs between processes costs about 100 us "
                   "per frame (the GPU time-slices between them). Start it with 'daoGpuMps start' "
                   "before the pipeline processes (DAO_GPU_NO_MPS_WARNING=1 hides this message).\n");
}

static int supports_fd_export(CUdevice dev)
{
    int vmm = 0, fd = 0;
    cu.DeviceGetAttribute(&vmm, CU_DEVICE_ATTRIBUTE_VIRTUAL_MEMORY_MANAGEMENT_SUPPORTED, dev);
    cu.DeviceGetAttribute(&fd, CU_DEVICE_ATTRIBUTE_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR_SUPPORTED, dev);
    return vmm && fd;
}

/* ---------------------------------------------------------------------------
 * public API
 * ------------------------------------------------------------------------- */
int daoShmGpuAvailable(void)
{
    int count = 0;
    CUdevice d;
    if (!driver_ok())
        return 0;
    if (cu.DeviceGetCount(&count) != CUDA_SUCCESS || count == 0)
        return 0;
    return cu.DeviceGet(&d, 0) == CUDA_SUCCESS && supports_fd_export(d);
}

int daoShmIsGpu(const IMAGE *image)
{
    return image && image->md && image->md[0].gpu_magic == DAO_GPU_MAGIC;
}

int_fast8_t daoShmCreateGpu(IMAGE *image, const char *name, long naxis, uint32_t *size,
                            uint8_t atype, int device, int NBkw, uint32_t flags)
{
    GpuState *st = NULL;
    CUmemAllocationProp prop;
    CUuuid uuid;
    size_t gran = 0;
    int fd = -1, created = 0, pushed = 0;
    struct stat file_stat;
    daoGpuMsg req, rep;
    uint64_t id;

    image->d_array = NULL;
    image->gpu = NULL;
    if (!driver_ok()) {
        daoError("GPU SHM %s: %s\n", name, load_error);
        return DAO_ERROR;
    }
    if (!elem_size(atype)) {
        daoError("GPU SHM %s: unknown data type %d\n", name, (int) atype);
        return DAO_ERROR;
    }
    st = calloc(1, sizeof *st);
    CU_TRY(cu.DeviceGet(&st->dev, device));
    if (!supports_fd_export(st->dev)) {
        daoError("GPU SHM %s: device %d cannot export memory as a file descriptor\n", name, device);
        goto fail;
    }
    CU_TRY(cu.DeviceGetUuid(&uuid, st->dev));
    CU_TRY(cu.DevicePrimaryCtxRetain(&st->ctx, st->dev));

    /* make sure the daemon runs before creating anything */
    daoGpuMsgInit(&req, DAO_GPU_OP_PING, NULL);
    if (!daemon_call(&req, -1, &rep, &fd, 1))
        goto fail;

    if (daoShmCreate(image, name, naxis, size, atype, 1, NBkw) != DAO_SUCCESS)
        goto fail;
    created = 1;
    image->d_array = NULL;       /* daoShmCreate resets the GPU fields */
    image->gpu = NULL;

    if (!push(st))
        goto fail;
    pushed = 1;
    st->bytes = (size_t) image->md[0].nelement * elem_size(atype);
    prop = alloc_prop(st->dev);
    CU_TRY(cu.MemGetAllocationGranularity(&gran, &prop, CU_MEM_ALLOC_GRANULARITY_MINIMUM));
    st->size = ((st->bytes + gran - 1) / gran) * gran;
    CU_TRY(cu.MemCreate(&st->handle, st->size, &prop, 0));
    if (!map_handle(st))
        goto fail;
    CU_TRY(cu.MemsetD8(st->dptr, 0, st->size));
    CU_TRY(cu.StreamSynchronize(0));
    CU_TRY(cu.MemExportToShareableHandle(&fd, st->handle, CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR, 0));

    /* hand the allocation to the daemon: from now on it outlives this process */
    if (stat(name, &file_stat) != 0) {
        daoError("GPU SHM %s: cannot stat the SHM file\n", name);
        goto fail;
    }
    id = new_gpu_id();
    daoGpuMsgInit(&req, DAO_GPU_OP_PUT, name);
    req.id = id;
    req.size = st->size;
    req.ino = (uint64_t) file_stat.st_ino;
    req.pid = (int32_t) getpid();
    {
        int unused = -1;
        if (!daemon_call(&req, fd, &rep, &unused, 1) || rep.op != DAO_GPU_OP_OK) {
            daoError("GPU SHM %s: daoGpuShmd refused the payload: %s\n", name, rep.text);
            goto fail;
        }
    }
    close(fd);
    fd = -1;

    st->host = image->array.V;
    st->mirror = (flags & DAO_GPU_MIRROR) != 0;
    if (st->mirror)
        register_host(st);
    pop();
    pushed = 0;

    image->md[0].gpu_device = device;
    memcpy(image->md[0].gpu_uuid, uuid.bytes, 16);
    image->md[0].gpu_flags = flags;
    image->md[0].gpu_size = st->size;
    image->md[0].gpu_id = id;
    __atomic_store_n(&image->md[0].gpu_magic, DAO_GPU_MAGIC, __ATOMIC_RELEASE);  /* last */

    image->d_array = (void *) st->dptr;
    image->gpu = st;
    daoInfo("GPU SHM %s: %zu B on device %d%s\n", name, st->bytes, device,
            st->mirror ? ", mirrored in /tmp" : "");
    warn_if_no_mps();
    return DAO_SUCCESS;

fail:
    if (fd >= 0)
        close(fd);
    if (pushed) {
        release_state(st);
        pop();
    } else if (st) {
        free(st);
    }
    if (created) {
        image->gpu = NULL;
        image->d_array = NULL;
        daoShmClose(image);
        unlink(name);
    }
    return DAO_ERROR;
}

int_fast8_t daoGpuAttach(IMAGE *image)
{
    IMAGE_METADATA *md = image->md;
    int mirror = (md[0].gpu_flags & DAO_GPU_MIRROR) != 0;
    GpuState *st = NULL;
    daoGpuMsg req, rep;
    int fd = -1, pushed = 0;

    image->d_array = NULL;
    image->gpu = NULL;
    if (!driver_ok()) {
        if (mirror) {
            daoWarning("%s is a GPU SHM, but %s: only its /tmp copy is readable\n", image->name, load_error);
            return DAO_SUCCESS;
        }
        daoError("%s is a GPU SHM, but %s\n", image->name, load_error);
        return DAO_ERROR;
    }
    st = calloc(1, sizeof *st);
    if (!device_by_uuid(md[0].gpu_uuid, &st->dev)) {
        daoError("%s: its GPU is not visible to this process (CUDA_VISIBLE_DEVICES?)\n", image->name);
        goto fail;
    }
    CU_TRY(cu.DevicePrimaryCtxRetain(&st->ctx, st->dev));

    daoGpuMsgInit(&req, DAO_GPU_OP_GET, image->name);
    if (!daemon_call(&req, -1, &rep, &fd, 0))
        goto fail;
    if (rep.op != DAO_GPU_OP_OK || fd < 0) {
        daoError("%s: daoGpuShmd: %s\n", image->name, rep.text);
        goto fail;
    }
    if (rep.id != md[0].gpu_id) {
        daoError("%s: the GPU payload held by daoGpuShmd belongs to another creation of this SHM\n",
                 image->name);
        goto fail;
    }
    if (!push(st))
        goto fail;
    pushed = 1;
    st->size = md[0].gpu_size;
    st->bytes = (size_t) md[0].nelement * elem_size(md[0].atype);
    CU_TRY(cu.MemImportFromShareableHandle(&st->handle, (void *) (uintptr_t) fd,
                                           CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR));
    close(fd);
    fd = -1;
    if (!map_handle(st))
        goto fail;
    st->host = image->array.V;
    st->mirror = mirror;
    if (mirror)
        register_host(st);
    pop();

    image->d_array = (void *) st->dptr;
    image->gpu = st;
    warn_if_no_mps();
    return DAO_SUCCESS;

fail:
    if (fd >= 0)
        close(fd);
    if (pushed) {
        release_state(st);
        pop();
    } else {
        free(st);
    }
    return DAO_ERROR;
}

void daoGpuDetach(IMAGE *image)
{
    GpuState *st = state_of(image);
    if (!st)
        return;
    if (push(st)) {
        release_state(st);
        pop();
    }
    image->gpu = NULL;
    image->d_array = NULL;
}

static int check_write(IMAGE *image, uint32_t nbVal, size_t *bytes)
{
    GpuState *st = state_of(image);
    if (!st) {
        daoError("%s: GPU SHM not accessible from this process\n", image->name);
        return 0;
    }
    if (image->md[0].fifo_size != 1) {
        daoError("%s: FIFO GPU SHMs are not supported\n", image->name);
        return 0;
    }
    *bytes = (size_t) nbVal * elem_size(image->md[0].atype);
    if (*bytes > st->bytes) {
        daoError("%s: %u values do not fit in the SHM\n", image->name, nbVal);
        return 0;
    }
    return 1;
}

int_fast8_t daoGpuSetData(IMAGE *image, const void *im, uint32_t nbVal, int post)
{
    GpuState *st = state_of(image);
    size_t bytes;
    CUresult r;
    if (!check_write(image, nbVal, &bytes))
        return DAO_ERROR;
    image->md[0].write = 1;
    if (st->mirror)
        memcpy(st->host, im, bytes);
    if (!push(st))
        return DAO_ERROR;
    /* From pageable memory, cuMemcpyHtoD may return before the DMA ends:
     * wait, so that readers woken below (or after this process exits) see it. */
    r = cu.MemcpyHtoD(st->dptr, im, bytes);
    if (r == CUDA_SUCCESS)
        r = cu.StreamSynchronize(0);
    pop();
    if (r != CUDA_SUCCESS) {
        daoError("%s: copy to the GPU failed: %s\n", image->name, cuerr(r));
        image->md[0].write = 0;
        return DAO_ERROR;
    }
    if (post)
        return daoShmSetDataPartFinalize(image);
    image->md[0].write = 0;
    return DAO_SUCCESS;
}

/* ---------------------------------------------------------------------------
 * consistent reads
 *
 * A GPU writer marks the frame with md[0].write = 1 (daoShmBeginWrite, or
 * daoShmSetData/daoShmCommit) and publishing clears it and increments cnt0.
 * A copy is kept only if no write was marked when it started, none when it
 * ended, and cnt0 did not change in between; otherwise it is retried.
 * ------------------------------------------------------------------------- */
#define READ_TIMEOUT_S 1.0

static double mono_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + 1e-9 * t.tv_nsec;
}

static int writing(IMAGE *image)
{
    return __atomic_load_n(&image->md[0].write, __ATOMIC_ACQUIRE) != 0;
}

static uint64_t counter(IMAGE *image)
{
    return __atomic_load_n(&image->md[0].cnt0, __ATOMIC_ACQUIRE);
}

static int_fast8_t consistent_copy(IMAGE *image, GpuState *st, void *dst, size_t bytes)
{
    double deadline = mono_s() + READ_TIMEOUT_S;
    CUresult r;
    if (!push(st))
        return DAO_ERROR;
    for (;;) {
        uint64_t before;
        while (writing(image) && mono_s() < deadline) {
            struct timespec ts = { 0, 20 * 1000 };
            nanosleep(&ts, NULL);
        }
        if (mono_s() >= deadline)
            break;
        before = counter(image);
        r = cu.MemcpyDtoH(dst, st->dptr, bytes);    /* synchronous */
        if (r != CUDA_SUCCESS) {
            pop();
            daoError("%s: copy from the GPU failed: %s\n", image->name, cuerr(r));
            return DAO_ERROR;
        }
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (!writing(image) && counter(image) == before) {
            pop();
            return DAO_SUCCESS;
        }
    }
    /* A writer that died between daoShmBeginWrite and publishing leaves the
     * mark set: return the current data rather than hang. */
    daoWarning("%s: no complete frame within %.0f s (writer stopped in the middle of a write?); "
               "returning the current data\n", image->name, READ_TIMEOUT_S);
    r = cu.MemcpyDtoH(dst, st->dptr, bytes);
    pop();
    return r == CUDA_SUCCESS ? DAO_SUCCESS : DAO_ERROR;
}

int_fast8_t daoGpuRefreshHost(IMAGE *image)
{
    GpuState *st = state_of(image);
    if (image->md[0].gpu_flags & DAO_GPU_MIRROR)
        return DAO_SUCCESS;                 /* the writer keeps it current */
    if (!st) {
        daoError("%s: GPU SHM not accessible from this process\n", image->name);
        return DAO_ERROR;
    }
    return consistent_copy(image, st, st->host, st->bytes);
}

/* ---------------------------------------------------------------------------
 * publishing
 * ------------------------------------------------------------------------- */
static void CUDA_CB commit_callback(void *arg)
{
    daoShmSetDataPartFinalize((IMAGE *) arg);   /* no CUDA call allowed here */
}

int_fast8_t daoShmCommit(IMAGE *image, void *stream)
{
    GpuState *st = state_of(image);
    size_t bytes;
    if (!check_write(image, 0, &bytes))
        return DAO_ERROR;
    daoShmBeginWrite(image);                /* in case the writer did not */
    if (!push(st))
        return DAO_ERROR;
    if (st->mirror)
        CU_TRY(cu.MemcpyDtoHAsync(st->host, st->dptr, st->bytes, (CUstream) stream));
    CU_TRY(cu.LaunchHostFunc((CUstream) stream, commit_callback, image));
    pop();
    return DAO_SUCCESS;
fail:
    pop();
    image->md[0].write = 0;
    return DAO_ERROR;
}

int_fast8_t daoShmCommitSync(IMAGE *image, void *stream)
{
    GpuState *st = state_of(image);
    size_t bytes;
    if (!check_write(image, 0, &bytes))
        return DAO_ERROR;
    daoShmBeginWrite(image);
    if (!push(st))
        return DAO_ERROR;
    if (st->mirror)
        CU_TRY(cu.MemcpyDtoHAsync(st->host, st->dptr, st->bytes, (CUstream) stream));
    CU_TRY(cu.StreamSynchronize((CUstream) stream));
    pop();
    return daoShmSetDataPartFinalize(image);
fail:
    pop();
    image->md[0].write = 0;
    return DAO_ERROR;
}

int_fast8_t daoShmSetDataDevice(IMAGE *image, const void *d_src, uint32_t nbVal, void *stream)
{
    GpuState *st = state_of(image);
    size_t bytes;
    if (!check_write(image, nbVal, &bytes))
        return DAO_ERROR;
    daoShmBeginWrite(image);
    if (!push(st))
        return DAO_ERROR;
    CU_TRY(cu.MemcpyDtoDAsync(st->dptr, (CUdeviceptr) (uintptr_t) d_src, bytes, (CUstream) stream));
    pop();
    return daoShmCommit(image, stream);
fail:
    pop();
    image->md[0].write = 0;
    return DAO_ERROR;
}

int_fast8_t daoShmCopyToHost(IMAGE *image, void *dst, uint32_t nbVal)
{
    GpuState *st = state_of(image);
    size_t bytes = (size_t) nbVal * elem_size(image->md[0].atype);
    if (!daoShmIsGpu(image)) {
        daoError("%s is not a GPU SHM\n", image->name);
        return DAO_ERROR;
    }
    if (!st) {                              /* no GPU here: the mirror is the data */
        if (!(image->md[0].gpu_flags & DAO_GPU_MIRROR))
            return DAO_ERROR;
        memcpy(dst, image->array.V, bytes);
        return DAO_SUCCESS;
    }
    if (bytes > st->bytes)
        bytes = st->bytes;
    return consistent_copy(image, st, dst, bytes);
}

#else  /* ---------------- built without CUDA, or not Linux ---------------- */

static int_fast8_t no_gpu(const char *what)
{
    daoError("%s: libdao was built without GPU SHM support (CUDA not found at build time)\n", what);
    return DAO_ERROR;
}

int daoShmGpuAvailable(void) { return 0; }

int daoShmIsGpu(const IMAGE *image)
{
    return image && image->md && image->md[0].gpu_magic == DAO_GPU_MAGIC;
}

int_fast8_t daoShmCreateGpu(IMAGE *image, const char *name, long naxis, uint32_t *size,
                            uint8_t atype, int device, int NBkw, uint32_t flags)
{
    (void) image; (void) naxis; (void) size; (void) atype; (void) device; (void) NBkw; (void) flags;
    return no_gpu(name);
}

int_fast8_t daoGpuAttach(IMAGE *image)
{
    image->d_array = NULL;
    image->gpu = NULL;
    if (image->md[0].gpu_flags & DAO_GPU_MIRROR) {
        daoWarning("%s is a GPU SHM: only its /tmp copy is readable here\n", image->name);
        return DAO_SUCCESS;
    }
    return no_gpu(image->name);
}

void daoGpuDetach(IMAGE *image) { (void) image; }

int_fast8_t daoGpuSetData(IMAGE *image, const void *im, uint32_t nbVal, int post)
{
    (void) im; (void) nbVal; (void) post;
    return no_gpu(image->name);
}

int_fast8_t daoGpuRefreshHost(IMAGE *image)
{
    return (image->md[0].gpu_flags & DAO_GPU_MIRROR) ? DAO_SUCCESS : no_gpu(image->name);
}

int_fast8_t daoShmCommit(IMAGE *image, void *stream) { (void) stream; return no_gpu(image->name); }

int_fast8_t daoShmCommitSync(IMAGE *image, void *stream) { (void) stream; return no_gpu(image->name); }

int_fast8_t daoShmSetDataDevice(IMAGE *image, const void *d_src, uint32_t nbVal, void *stream)
{
    (void) d_src; (void) nbVal; (void) stream;
    return no_gpu(image->name);
}

int_fast8_t daoShmCopyToHost(IMAGE *image, void *dst, uint32_t nbVal)
{
    (void) dst; (void) nbVal;
    return no_gpu(image->name);
}

#endif
