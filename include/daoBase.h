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

char * daoBaseGetTimeStamp();
#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)

int daoLogLevel = 1;
#define daoError(fmt, ...) fprintf(stderr, "%s [error] %s:%d: " fmt, daoBaseGetTimeStamp(), __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define	daoPrint(fmt, ...) \
            do { fprintf(stdout, "%s:%d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define	daoWarning(fmt, ...) \
            do { if (daoLogLevel>=DAO_WARNING) fprintf(stdout, "%s [warning] %s:%s:%d: " fmt, daoBaseGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define	daoInfo(fmt, ...) \
            do { if (daoLogLevel>=DAO_INFO) fprintf(stdout, "%s [info] %s:%s:%d: " fmt, daoBaseGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define	daoDebug(fmt, ...) \
            do { if (daoLogLevel>=DAO_DEBUG) fprintf(stdout, "%s [debug] %s:%s:%d: " fmt, daoBaseGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while(0)
#define daoTrace(fmt, ...) \
            do { if (daoLogLevel>=DAO_TRACE) fprintf(stdout, "%s [trace] %s:%s:%d " fmt, daoBaseGetTimeStamp(), __FILENAME__, __FUNCTION__, __LINE__, ##__VA_ARGS__); } while (0)


unsigned daoBaseIp2Int(const char * ip); 

#endif
