/**
 * @file    daoLog2_wrapper.hpp
 * @brief   RT Log to send to protobuf
 *
 *
 * @author  D. Barr
 * @date    19 October 2023
 *
 * @bug No known bugs.
 *
 */

#ifndef DAO_LOG2_WRAPPER_HPP
#define DAO_LOG2_WRAPPER_HPP

#ifdef __cplusplus
extern "C" {
#endif

struct daoLog;
typedef struct daoLog daoLog_t;

daoLog_t * daoLog_create();
void daoLog_destroy(daoLog_t * m);


void daoLogDebug(daoLog_t * m, const char* fmt, ...);
void daoLogInfo(daoLog_t * m, const char* fmt, ...);
void daoLogWarning(daoLog_t * m, const char* fmt, ...);
void daoLogError(daoLog_t * m, const char* fmt, ...);
void daoLogCritical(daoLog_t * m, const char* fmt, ...);

void daoLogSetLevel(daoLog_t * m, int level);
int daoLogGetLevel(daoLog_t * m);

#ifdef __cplusplus
}
#endif

#endif /* __MATHER_H__ */
