/**
 * @file    daoBase.c
 * @brief   Durham AO RTC library
 * 
 * Durham AO RTC library description file. 
 *  
 * @author  S. Cetre
 * @date    23/06/2022
 *
 * 
 */

#ifndef _DAOBASE_H
#define _DAOBASE_H

#define DAO_SUCCESS 0
#define DAO_ERROR   1

#define DAO_WARNING 0
#define DAO_INFO 1
#define DAO_DEBUG 2
#define DAO_TRACE 3

#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

int daoLogLevel = 1;
#define daoError(fmt, ...) fprintf(stderr, "%s [error] %s:%d: " fmt, daoGetTimeStamp(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define	daoPrint(fmt, ...) \
            do { fprintf(stdout, "%s:%d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define	daoWarning(fmt, ...) \
            do { if (daoLogLevel>=DAO_WARNING) fprintf(stdout, "%s [warning] %s:%s:%d: " fmt, daoGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define	daoInfo(fmt, ...) \
            do { if (daoLogLevel>=DAO_INFO) fprintf(stdout, "%s [info] %s:%s:%d: " fmt, daoGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define	daoDebug(fmt, ...) \
            do { if (daoLogLevel>=DAO_DEBUG) fprintf(stdout, "%s [debug] %s:%s:%d: " fmt, daoGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define daoTrace(fmt, ...) \
            do { if (daoLogLevel>=DAO_TRACE) fprintf(stdout, "%s [trace] %s:%s:%d " fmt, daoGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while (0)


#include "daoImageStruct.h" // data structure

char * daoGetTimeStamp();

int_fast8_t daoInit1D(const char *name, char *prefix, uint32_t nbVal, IMAGE **image);
int_fast8_t daoShm2Img(const char *name, char *prefix, IMAGE *image);

//int_fast8_t daoImage2Shm(float *procim, int nbVal, IMAGE *image); 
int_fast8_t daoImage2Shm(void *procim, uint32_t nbVal, IMAGE *image); 

int_fast8_t daoImagePart2Shm(char *procim, uint32_t nbVal, IMAGE *image, uint32_t position,
                             uint16_t packetId, uint16_t packetTotal, uint64_t frameNumber); 
int_fast8_t daoImagePart2ShmFinalize(IMAGE *image); 

int_fast8_t daoImageCreateSem(IMAGE *image, long NBsem);
int_fast8_t daoImageCreate(IMAGE *image, const char *name, long naxis, uint32_t *size, uint8_t atype, int shared, int NBkw);

//int_fast8_t daoImage2ShmUI8(unsigned char *procim, int nbVal, IMAGE *image); 
//int_fast8_t daoImage2ShmUI16(unsigned short *procim, int nbVal, IMAGE *image); 
//int_fast8_t daoImage2ShmSI8(char *procim, int nbVal, IMAGE *image); 
//int_fast8_t daoImage2ShmSI16(short *procim, int nbVal, IMAGE *image); 
//int_fast8_t daoImage2ShmUI32(unsigned int *procim, int nbVal, IMAGE *image); 
//int_fast8_t daoImage2ShmSI32(int *procim, int nbVal, IMAGE *image); 

#endif
