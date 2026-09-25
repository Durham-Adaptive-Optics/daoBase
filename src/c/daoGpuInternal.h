/**
 * @file    daoGpuInternal.h
 * @brief   Hooks of dao.c into daoGpu.c for GPU SHMs (internal, not installed).
 */
#ifndef DAO_GPU_INTERNAL_H
#define DAO_GPU_INTERNAL_H

#include "dao.h"

#if defined(__GNUC__) && !defined(_WIN32)
#define DAO_GPU_INTERNAL __attribute__((visibility("hidden")))
#else
#define DAO_GPU_INTERNAL
#endif

/* daoShmOpen of a GPU SHM: map the payload held by daoGpuShmd. */
DAO_GPU_INTERNAL int_fast8_t daoGpuAttach(IMAGE *image);
/* daoShmClose: unmap this process's view of the payload. */
DAO_GPU_INTERNAL void        daoGpuDetach(IMAGE *image);
/* daoShmSetData / daoShmSetDataQuiet (post = 0) of a GPU SHM. */
DAO_GPU_INTERNAL int_fast8_t daoGpuSetData(IMAGE *image, const void *im, uint32_t nbVal, int post);
/* daoShmGetData*: make the /tmp host copy current when it is not mirrored. */
DAO_GPU_INTERNAL int_fast8_t daoGpuRefreshHost(IMAGE *image);

#endif /* DAO_GPU_INTERNAL_H */
