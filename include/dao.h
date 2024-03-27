/**
 * @file    dao.h
 * @brief   Durham AO RTC SHM library
 * 
 * Durham AO RTC Shared memory library description file. Inspired by ImageStreamIO
 *  
 * @author  S. Cetre
 * @date    23/06/2022
 *
 */

#ifndef _DAO_H
#define _DAO_H

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

// BOTH old and new log system are available and maintained
// New Log System
#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_WARNING 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3
#define LOG_LEVEL_TRACE 4

#define DEFAULT_LOG_LEVEL LOG_LEVEL_INFO

void daoLog(int log_level, const char *format, ...);
void daoLogError(const char *format, ...);
void daoLogPrint(const char *format, ...);
void daoLogWarning(const char *format, ...);
void daoLogInfo(const char *format, ...);
void daoLogDebug(const char *format, ...);
void daoLogTrace(const char *format, ...);
void daoLogSetLevel(int log_level);

// original Log System
#define DAO_SUCCESS 0
#define DAO_ERROR   1

#define DAO_WARNING 0
#define DAO_INFO 1
#define DAO_DEBUG 2
#define DAO_TRACE 3

#define ANSI_COLOR_RED      "\x1b[31m"
#define ANSI_COLOR_ORANGE   "\x1b[38;5;208m"
#define ANSI_COLOR_GREEN    "\x1b[32m"
#define ANSI_COLOR_YELLOW   "\x1b[33m"
#define ANSI_COLOR_BLUE     "\x1b[34m"
#define ANSI_COLOR_RESET     "\x1b[0m"

char * daoBaseGetTimeStamp();
#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

int daoLogLevel = 1;
#define daoError(fmt, ...) fprintf(stderr, ANSI_COLOR_RESET ANSI_COLOR_BLUE "%s " ANSI_COLOR_RESET ANSI_COLOR_RED "[error]" ANSI_COLOR_RESET " %s:%d: " fmt, daoBaseGetTimeStamp(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define	daoPrint(fmt, ...) \
            do { fprintf(stdout, "%s:%d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define	daoWarning(fmt, ...) \
            do { if (daoLogLevel>=DAO_WARNING) fprintf(stdout, ANSI_COLOR_RESET ANSI_COLOR_BLUE "%s " ANSI_COLOR_RESET ANSI_COLOR_ORANGE "[warning]" ANSI_COLOR_RESET " %s:%s:%d: " fmt, daoBaseGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define	daoInfo(fmt, ...) \
            do { if (daoLogLevel>=DAO_INFO) fprintf(stdout,  ANSI_COLOR_RESET ANSI_COLOR_BLUE "%s " ANSI_COLOR_RESET ANSI_COLOR_GREEN "[info]" ANSI_COLOR_RESET " %s:%s:%d: " fmt, daoBaseGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define	daoDebug(fmt, ...) \
            do { if (daoLogLevel>=DAO_DEBUG) fprintf(stdout, ANSI_COLOR_RESET ANSI_COLOR_BLUE "%s " ANSI_COLOR_RESET ANSI_COLOR_YELLOW "[debug]" ANSI_COLOR_RESET " %s:%s:%d: " fmt, daoBaseGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define daoTrace(fmt, ...) \
            do { if (daoLogLevel>=DAO_TRACE) fprintf(stdout, ANSI_COLOR_RESET ANSI_COLOR_BLUE "%s " ANSI_COLOR_RESET "[trace] %s:%s:%d " fmt, daoBaseGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while (0)


// Since we are recompiling a library, using the ImageStreamIO structure, we need to define the DAO_COMPAT for the header file to match DAO
#define DAO_COMPAT
// Include Image structure from ImageStreamIO modules
#include "ImageStreamIO/ImageStruct.h"

#undef SHAREDMEMDIR
#define SHAREDMEMDIR        ""        /**< location of file mapped semaphores */
#undef SEMAPHORE_MAXVAL
#define SEMAPHORE_MAXVAL    1 	          /**< maximum value for each of the semaphore, mitigates warm-up time when processes catch up with data that has accumulated */
#undef CIRCULAR_BUFFER_SIZE
#define CIRCULAR_BUFFER_SIZE 1000         /**< Number of data in the cuircular buffer */

// Function declaration
void daoSetLogLevel(int logLevel);
unsigned daoBaseIp2Int(const char * ip); 


int_fast8_t daoShmInit1D(const char *name, uint32_t nbVal, IMAGE **image);
int_fast8_t daoShmShm2Img(const char *name, IMAGE *image);
int_fast8_t daoShmImage2Shm(void *im, uint32_t nbVal, IMAGE *image); 
int_fast8_t daoShmImagePart2Shm(char *im, uint32_t nbVal, IMAGE *image, uint32_t position,
                             uint16_t packetId, uint16_t packetTotal, uint64_t frameNumber); 
int_fast8_t daoShmImagePart2ShmFinalize(IMAGE *image); 
int_fast8_t daoShmImageCreateSem(IMAGE *image, long NBsem);
int_fast8_t daoShmImageCreate(IMAGE *image, const char *name, long naxis, uint32_t *size,
                           uint8_t datatype, int shared, int NBkw);
int_fast8_t daoShmCombineShm2Shm(IMAGE **imageCude, IMAGE *image, int nbChannel, int nbVal); 
int_fast8_t daoShmWaitForSemaphore(IMAGE *image, int32_t semNb);
int_fast8_t daoShmWaitForCounter(IMAGE *image);
uint64_t    daoShmGetCounter(IMAGE *image);
#endif