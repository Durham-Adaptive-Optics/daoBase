/*****************************************************************************
  DAO project
  RTC library funciton
  S.Cetre
 *****************************************************************************/

/*==========================================================================*/
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <semaphore.h>
#include <sched.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/file.h>
#include <errno.h>
#include <sys/mman.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/time.h>
#include <gsl/gsl_blas.h>
#include <omp.h>

#ifdef __MACH__
#include <mach/mach_time.h>
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
static int clock_gettime(int clk_id, struct mach_timespec *t)
{
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    uint64_t time;
    time = mach_absolute_time();
    double nseconds = ((double)time * (double)timebase.numer)/((double)timebase.denom);
    double seconds = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
    t->tv_sec = seconds;
    t->tv_nsec = nseconds;
    return 0;
}
#else
#include <time.h>
#endif

#include "dao.h"
static int current_log_level = DEFAULT_LOG_LEVEL;

// Include Image structure from ImageStreamIO modules
#include "ImageStreamIO/ImageStruct.h"
/**
 * @brief Return a time stamp string with microsecond precision 
 * 
 * @return char* 
 */
char * daoBaseGetTimeStamp()
{
    struct timespec curTime;
    clock_gettime(CLOCK_REALTIME, &curTime);
    int micro = (int)((double)curTime.tv_nsec/1000);
    char buffer [80];
    strftime(buffer, 80, "%Y-%m-%d_%H:%M:%S", localtime(&curTime.tv_sec));
    static char currentTime[128] = "";
    sprintf(currentTime, "%s:%d", buffer, micro);
    return currentTime;
}

/**
 * @brief 
 * 
 * @param ip 
 * @return unsigned 
 */
unsigned daoBaseIp2Int (const char * ip)
{
    /* The return value. */
    unsigned v = 0;
    /* The count of the number of bytes processed. */
    int i;
    /* A pointer to the next digit to process. */
    const char * start;

    start = ip;
    for (i = 0; i < 4; i++) {
        /* The digit being processed. */
        char c;
        /* The value of this byte. */
        int n = 0;
        while (1) {
            c = * start;
            start++;
            if (c >= '0' && c <= '9') {
                n *= 10;
                n += c - '0';
            }
            /* We insist on stopping at "." if we are still parsing
               the first, second, or third numbers. If we have reached
               the end of the numbers, we will allow any character. */
            else if ((i < 3 && c == '.') || i == 3) {
                break;
            }
            else {
                return DAO_ERROR;
            }
        }
        if (n >= 256) {
            return DAO_ERROR;
        }
        v *= 256;
        v += n;
    }
    return v;
}


/*
 * Log a message with the specified log level and format string.
 * If the log level is higher than the current log level, the message
 * is not logged.
 */
void daoLog(int log_level, const char *format, ...) 
{
    if (log_level > current_log_level) 
    {
        return;
    }

    // Get the current time and format it as a string
    /*time_t current_time;
    char* c_time_string;
    current_time = time(NULL);
    c_time_string = ctime(&current_time);
    c_time_string[strlen(c_time_string)-1] = '\0';*/

    // Map the log level to a string representation
    char* log_level_string;
    switch (log_level) {
        case LOG_LEVEL_ERROR:
            log_level_string = "ERROR";
            break;
        case LOG_LEVEL_WARNING:
            log_level_string = "WARNING";
            break;
        case LOG_LEVEL_INFO:
            log_level_string = "INFO";
            break;
        case LOG_LEVEL_DEBUG:
            log_level_string = "DEBUG";
            break;
        case LOG_LEVEL_TRACE:
            log_level_string = "TRACE";
            break;
        default:
            log_level_string = "UNKNOWN";
            break;
    }

    // Print the log message to the console
    va_list args;
    va_start(args, format);
    printf("[%s] [%s] ", daoBaseGetTimeStamp(), log_level_string);
    vprintf(format, args);
    va_end(args);
}

/*
 * Log an error message with the specified format string.
 */
void daoLogError(const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    daoLog(LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

/*
 * Log a print message with the specified format string.
 */
void daoLogPrint(const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    daoLog(LOG_LEVEL_INFO, format, args);
    va_end(args);
}

/*
 * Log a warning message with the specified format string.
 */
void daoLogWarning(const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    daoLog(LOG_LEVEL_WARNING, format, args);
    va_end(args);
}

/*
 * Log an info message with the specified format string.
 */
void daoLogInfo(const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    daoLog(LOG_LEVEL_INFO, format, args);
    va_end(args);
}

/*
 * Log a debug message with the specified format string.
 */
void daoLogDebug(const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    daoLog(LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

/*
 * Log a trace message with the specified format string.
 */
void daoLogTrace(const char *format, ...) 
{
    va_list args;
    va_start(args, format);
    daoLog(LOG_LEVEL_TRACE, format, args);
    va_end(args);
}


/*
 * Set the current log level.
 */
void daoLogSetLevel(int log_level) 
{
    current_log_level = log_level;
}

/*
 * Set the current log level.
 */
void daoSetLogLevel(int logLevel) 
{
    daoLogLevel = logLevel;
}

// SHM
int NBIMAGES = 10;

#include <stdbool.h>
bool extract_name(const char *path, char *name)
{
    const char *prefix = "/milk/shm/";
    const char *suffix = ".im.shm";

    // Check if the path starts with the prefix
    if (strncmp(path, prefix, strlen(prefix)) != 0) 
    {
        return false;
    }

    // Check if the path ends with the suffix
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);
    if (path_len < suffix_len || strcmp(path + path_len - suffix_len, suffix) != 0) 
    {
        return false;
    }

    // Calculate the start and end of the name in the path
    const char *name_start = path + strlen(prefix);
    const char *name_end = path + path_len - suffix_len;

    // Check if the name part is not empty
    if (name_start >= name_end) 
    {
        return false;
    }

    // Copy the name part
    size_t name_len = name_end - name_start;
    strncpy(name, name_start, name_len);
    name[name_len] = '\0';  // Null terminate the extracted name

    return true;
}

/**
 * Extract image from a shared memory
 */
//int_fast8_t daoShmShm2Img(const char *name, char *prefix, IMAGE *image)
int_fast8_t daoShmShm2Img(const char *name, IMAGE *image)
{
    daoTrace("\n");
    char extracted_name[100];
    if (extract_name(name, extracted_name))
    {
        ImageStreamIO_openIm(image, extracted_name);
    }
    else 
    {
        daoError("Wrong file name, ensure it is using the /milk/shm/<name>.im.shm\n");
        return DAO_ERROR;
    }
    return DAO_SUCCESS;
//    int shmFd;
//    struct stat file_stat;
//    char shmName[256];
//    IMAGE_METADATA *map;
//    char *mapv;
//    uint8_t datatype;
//    int kw;
//    char shmSemName[256];
//    sem_t *stest;
//    int sOK;
//    long snb;
//    long s;
//
//    int rval = DAO_ERROR;
//    sprintf(shmName, "%s", name);
//    //sprintf(shmName, "%s/%s%s.im.shm", SHAREDMEMDIR, prefix, name);
//
//    shmFd = open(shmName, O_RDWR);
//    if(shmFd==-1)
//    {
//        image->used = 0;
//        daoError("Cannot import shared memory file %s \n", name);
//        rval = DAO_ERROR;
//    }
//    else
//    {
//        rval = DAO_SUCCESS; // we assume by default success
//
//        fstat(shmFd, &file_stat);
//        daoDebug("File %s size: %zd\n", shmName, file_stat.st_size);
//        map = (IMAGE_METADATA*) mmap(0, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
//        if (map == MAP_FAILED) 
//        {
//            close(shmFd);
//            perror("Error mmapping the file");
//            rval = DAO_ERROR;
//            exit(0);
//        }
//
//        image->memsize = file_stat.st_size;
//        image->shmfd = shmFd;
//        image->md = map;
//        //        image->md[0].sem = 0;
//        datatype = image->md[0].datatype;
//        image->md[0].shared = 1;
//
//        daoDebug("image size = %ld %ld\n", (long) image->md[0].size[0], (long) image->md[0].size[1]);
//        fflush(stdout);
//        // some verification
////        if(image->md[0].size[0]*image->md[0].size[1]>10000000000)
////        {
////            daoDebug("IMAGE \"%s\" SEEMS BIG... ABORTING\n", name);
////            rval = DAO_ERROR;
////            exit(0);
////        }
//        if(image->md[0].size[0]<1)
//        {
//            daoError("IMAGE \"%s\" AXIS SIZE < 1... ABORTING\n", name);
//            rval = DAO_ERROR;
//            exit(0);
//        }
//        if(image->md[0].size[1]<1)
//        {
//            daoError("IMAGE \"%s\" AXIS SIZE < 1... ABORTING\n", name);
//            rval = DAO_ERROR;
//            exit(0);
//        }
//
//        mapv = (char*) map;
//        mapv += sizeof(IMAGE_METADATA);
//
//        daoDebug("datatype = %d\n", (int) datatype);
//        fflush(stdout);
//
//        if(datatype == _DATATYPE_UINT8)
//        {
//            daoDebug("datatype = UINT8\n");
//            image->array.UI8 = (uint8_t*) mapv;
//            mapv += SIZEOF_DATATYPE_UINT8 * image->md[0].nelement;
//        }
//        if(datatype == _DATATYPE_INT8)
//        {
//            daoDebug("datatype = INT8\n");
//            image->array.SI8 = (int8_t*) mapv;
//            mapv += SIZEOF_DATATYPE_INT8 * image->md[0].nelement;
//        }
//        if(datatype == _DATATYPE_UINT16)
//        {
//            daoDebug("datatype = UINT16\n");
//            image->array.UI16 = (uint16_t*) mapv;
//            mapv += SIZEOF_DATATYPE_UINT16 * image->md[0].nelement;
//        }
//        if(datatype == _DATATYPE_INT16)
//        {
//            daoDebug("datatype = INT16\n");
//            image->array.SI16 = (int16_t*) mapv;
//            mapv += SIZEOF_DATATYPE_INT16 * image->md[0].nelement;
//        }
//        if(datatype == _DATATYPE_UINT32)
//        {
//            daoDebug("datatype = UINT32\n");
//            image->array.UI32 = (uint32_t*) mapv;
//            mapv += SIZEOF_DATATYPE_UINT32 * image->md[0].nelement;
//        }
//        if(datatype == _DATATYPE_INT32)
//        {
//            daoDebug("datatype = INT32\n");
//            image->array.SI32 = (int32_t*) mapv;
//            mapv += SIZEOF_DATATYPE_INT32 * image->md[0].nelement;
//        }
//        if(datatype == _DATATYPE_UINT64)
//        {
//            daoDebug("datatype = UINT64\n");
//            image->array.UI64 = (uint64_t*) mapv;
//            mapv += SIZEOF_DATATYPE_UINT64 * image->md[0].nelement;
//        }
//        if(datatype == _DATATYPE_INT64)
//        {
//            daoDebug("datatype = INT64\n");
//            image->array.SI64 = (int64_t*) mapv;
//            mapv += SIZEOF_DATATYPE_INT64 * image->md[0].nelement;
//        }
//        if(datatype == _DATATYPE_FLOAT)
//        {
//            daoDebug("datatype = FLOAT\n");
//            image->array.F = (float*) mapv;
//            mapv += SIZEOF_DATATYPE_FLOAT * image->md[0].nelement;
//        }
//        if(datatype == _DATATYPE_DOUBLE)
//        {
//            daoDebug("datatype = DOUBLE\n");
//            image->array.D = (double*) mapv;
//            mapv += SIZEOF_DATATYPE_COMPLEX_DOUBLE * image->md[0].nelement;
//        }
//        if(datatype == _DATATYPE_COMPLEX_FLOAT)
//        {
//            daoDebug("datatype = COMPLEX_FLOAT\n");
//            image->array.CF = (complex_float*) mapv;
//            mapv += SIZEOF_DATATYPE_COMPLEX_FLOAT * image->md[0].nelement;
//        }
//        if(datatype == _DATATYPE_COMPLEX_DOUBLE)
//        {
//            daoDebug("datatype = COMPLEX_DOUBLE\n");
//            image->array.CD = (complex_double*) mapv;
//            mapv += SIZEOF_DATATYPE_COMPLEX_DOUBLE * image->md[0].nelement;
//        }
//        daoDebug("%ld keywords\n", (long) image->md[0].NBkw);
//        fflush(stdout);
//
//        image->kw = (IMAGE_KEYWORD*) (mapv);
//        for(kw=0; kw<image->md[0].NBkw; kw++)
//        {
//            if(image->kw[kw].type == 'L')
//                daoDebug("%d  %s %ld %s\n", kw, image->kw[kw].name, image->kw[kw].value.numl, image->kw[kw].comment);
//            if(image->kw[kw].type == 'D')
//                daoDebug("%d  %s %lf %s\n", kw, image->kw[kw].name, image->kw[kw].value.numf, image->kw[kw].comment);
//            if(image->kw[kw].type == 'S')
//                daoDebug("%d  %s %s %s\n", kw, image->kw[kw].name, image->kw[kw].value.valstr, image->kw[kw].comment);
//        }
//
//        mapv += sizeof(IMAGE_KEYWORD)*image->md[0].NBkw;
//        strcpy(image->name, name);
//        // to get rid off warning
//        char * nameCopy = (char *)malloc(strlen(name)*sizeof(char));
//        strcpy(nameCopy, name);
//        char *token = strtok(nameCopy, "/");
//
//        char *semFName = NULL;
//        char *localName = NULL;
//
//        while (token != NULL)
//        {
//            semFName = token;
//            token = strtok(NULL, "/");
//        }
//
//        if (semFName != NULL)
//        {
//            daoInfo("shm name = %s\n", semFName);
//            localName = strtok(semFName, ".");
//            if (localName != NULL)
//            {
//                daoInfo("local name: %s\n", localName);
//            }
//            else
//            {
//                daoError("invalid name format, should be <name.im.shm>\n");
//                free(nameCopy);
//                return DAO_ERROR;
//            }
//        }
//        else
//        {
//            daoError("invalid location, should be /tmp/<optional>/<name.im.shm>\n");
//            free(nameCopy);
//            return DAO_ERROR;
//        }
//        // looking for semaphores
//        snb = 0;
//        sOK = 1;
//        while(sOK==1)
//        {
//            sprintf(shmSemName, "%s_sem%02ld", localName, snb);
//            daoDebug("semaphore %s\n", shmSemName);
//            if((stest = sem_open(shmSemName, 0, 0644, 0))== SEM_FAILED)
//            {
//                sOK = 0;
//            }
//            else
//            {
//                sem_close(stest);
//                snb++;
//            }
//        }
//        daoDebug("%ld semaphores detected  (image->md[0].sem = %d)\n", snb, (int) image->md[0].sem);
//        //        image->md[0].sem = snb;
//        image->semptr = (sem_t**) malloc(sizeof(sem_t*) * image->md[0].sem);
//        for(s=0; s<snb; s++)
//        {
//            sprintf(shmSemName, "%s_sem%02ld", localName, s);
//            if ((image->semptr[s] = sem_open(shmSemName, 0, 0644, 0))== SEM_FAILED) 
//            {
//                daoError("could not open semaphore %s\n", shmSemName);
//            }
//        }
//
//        sprintf(shmSemName, "%s_semlog", localName);
//        if ((image->semlog = sem_open(shmSemName, 0, 0644, 0))== SEM_FAILED) 
//        {
//            daoWarning("could not open semaphore %s\n", shmSemName);
//        }
//        free(nameCopy);
//    }
//    return(rval);
//    return DAO_SUCCESS;
}

/**
 * Init 1D array in shared memory
 */
int_fast8_t daoShmInit1D(const char *name, uint32_t nbVal, IMAGE **image)
{
    daoTrace("\n");
    int naxis = 2;
    char fullName[64];
    sprintf(fullName, "%s", name);

    daoDebug("daoInit1D(%s, %i)\n", fullName, nbVal);
    uint32_t imsize[2];
    imsize[0] = nbVal;
    imsize[1] = 1;

    daoDebug("\tdaoInit1D, imsize set\n");
    daoDebug("sizeof(image) = %ld\n", sizeof(IMAGE));
    *image = (IMAGE*)malloc(sizeof(IMAGE)*NBIMAGES);

    if(*image == NULL)
    {
        daoError("   OS Declined to allocate requested memory\n");
        exit(-1);
    }
    daoInfo("Alloc ok\n");

    memset(*image, 0, sizeof(IMAGE)*NBIMAGES);
    daoDebug("ECHO %i, %i\n", imsize[0], NBIMAGES);

    daoShmImageCreate(*image, fullName, naxis, imsize, _DATATYPE_FLOAT, 1, 0);

    return DAO_SUCCESS;
}

/*
 * The image is send to the shared memory.
 */
int_fast8_t daoShmImage2Shm(void *im, uint32_t nbVal, IMAGE *image) 
{
    daoTrace("\n");
    int semval = 0;
    int ss;
    image->md[0].write = 1;

    if (image->md[0].datatype == _DATATYPE_UINT8)
        memcpy(image->array.UI8, (unsigned char *)im, nbVal*sizeof(unsigned char)); 
    else if (image->md[0].datatype == _DATATYPE_INT8)
        memcpy(image->array.SI8, (char *)im, nbVal*sizeof(char));       
    else if (image->md[0].datatype == _DATATYPE_UINT16)
        memcpy(image->array.UI16, (unsigned short *)im, nbVal*sizeof(unsigned short));
    else if (image->md[0].datatype == _DATATYPE_INT16)
        memcpy(image->array.SI16, (short *)im, nbVal*sizeof(short));
    else if (image->md[0].datatype == _DATATYPE_INT32)
        memcpy(image->array.UI32, (unsigned int *)im, nbVal*sizeof(unsigned int));
    else if (image->md[0].datatype == _DATATYPE_UINT32)
        memcpy(image->array.SI32, (int *)im, nbVal*sizeof(int));
    else if (image->md[0].datatype == _DATATYPE_UINT64)
        memcpy(image->array.UI64, (unsigned long *)im, nbVal*sizeof(unsigned long));
    else if (image->md[0].datatype == _DATATYPE_INT64)
        memcpy(image->array.SI64, (long *)im, nbVal*sizeof(long));
    else if (image->md[0].datatype == _DATATYPE_FLOAT)
        memcpy(image->array.F, (float *)im, nbVal*sizeof(float));
    else if (image->md[0].datatype == _DATATYPE_DOUBLE)
        memcpy(image->array.D, (double *)im, nbVal*sizeof(double));

    for(ss = 0; ss < image->md[0].sem; ss++)
    {
        sem_getvalue(image->semptr[ss], &semval);
        if(semval < SEMAPHORE_MAXVAL )
            sem_post(image->semptr[ss]);
    }

    if(image->semlog != NULL)
    {
        sem_getvalue(image->semlog, &semval);
        if(semval < SEMAPHORE_MAXVAL)
        {
            sem_post(image->semlog);
        }
    }

    image->md[0].write = 0;
    image->md[0].cnt0++;
    clock_gettime(CLOCK_REALTIME, &image->md[0].atime);

    return DAO_SUCCESS;
}
/*
 * The image is send to the shared memory.
 * No release of semaphore since it is a part write
 */
int_fast8_t daoShmImagePart2Shm(char *im, uint32_t nbVal, IMAGE *image, uint32_t position,
                             uint16_t packetId, uint16_t packetTotal, uint64_t frameNumber) 
{
    daoTrace("\n");
    //int pp;
    image->md[0].write = 1;

    if (image->md[0].datatype == _DATATYPE_UINT8)
        memcpy(&image->array.UI8[position], (unsigned char *)im, nbVal*sizeof(unsigned char)); 
    else if (image->md[0].datatype == _DATATYPE_INT8)
        memcpy(&image->array.SI8[position], (char *)im, nbVal*sizeof(char));       
    else if (image->md[0].datatype == _DATATYPE_UINT16)
        memcpy(&image->array.UI16[position], (unsigned short *)im, nbVal*sizeof(unsigned short));
    else if (image->md[0].datatype == _DATATYPE_INT16)
        memcpy(&image->array.SI16[position], (short *)im, nbVal*sizeof(short));
    else if (image->md[0].datatype == _DATATYPE_INT32)
        memcpy(&image->array.UI32[position], (unsigned int *)im, nbVal*sizeof(unsigned int));
    else if (image->md[0].datatype == _DATATYPE_UINT32)
        memcpy(&image->array.SI32[position], (int *)im, nbVal*sizeof(int));
    else if (image->md[0].datatype == _DATATYPE_UINT64)
        memcpy(&image->array.UI64[position], (unsigned long *)im, nbVal*sizeof(unsigned long));
    else if (image->md[0].datatype == _DATATYPE_INT64)
        memcpy(&image->array.SI64[position], (long *)im, nbVal*sizeof(long));
    else if (image->md[0].datatype == _DATATYPE_FLOAT)
        memcpy(&image->array.F[position], (float *)im, nbVal*sizeof(float));
    else if (image->md[0].datatype == _DATATYPE_DOUBLE)
        memcpy(&image->array.D[position], (double *)im, nbVal*sizeof(double));

    image->md[0].lastPos = position;
    image->md[0].lastNb = nbVal;
    image->md[0].packetNb = packetId;
    image->md[0].packetTotal = packetTotal;
    image->md[0].lastNbArray[packetId] = frameNumber;
    image->md[0].write = 0;

    return DAO_SUCCESS;
}

/*
 * The image has beed sent to the shared memory.
 * Release of semaphore since it is a part write
 */
int_fast8_t daoShmImagePart2ShmFinalize(IMAGE *image) 
{
    daoTrace("\n");
    int semval = 0;
    int ss;
    for(ss = 0; ss < image->md[0].sem; ss++)
    {
        sem_getvalue(image->semptr[ss], &semval);
        if(semval < SEMAPHORE_MAXVAL )
            sem_post(image->semptr[ss]);
    }

    if(image->semlog != NULL)
    {
        sem_getvalue(image->semlog, &semval);
        if(semval < SEMAPHORE_MAXVAL)
        {
            sem_post(image->semlog);
        }
    }
    clock_gettime(CLOCK_REALTIME, &image->md[0].atime);
    image->md[0].cnt0++;
    return DAO_SUCCESS;
}

int_fast8_t daoImageCreateSem(IMAGE *image, long NBsem)
{
    daoTrace("\n");
    char shmSemName[256];
    long s, s1;
//    int r;
//    char command[256];
    char fname[256];
//    int semfile[100];

    // to get rid off warning
    char * nameCopy = (char *)malloc(strlen(image->md[0].name)*sizeof(char));
    strcpy(nameCopy, image->md[0].name);
    char *token = strtok(nameCopy, "/");

    char *semFName = NULL;
    char *localName = NULL; 

    while (token != NULL) 
    {
        semFName = token;
        token = strtok(NULL, "/");
    }
    if (semFName != NULL)
    {
        daoInfo("shm name = %s\n", semFName);   
        localName = strtok(semFName, ".");
        if (localName != NULL) 
        {
            daoInfo("local name: %s\n", localName);
        }
        else
        {
            daoError("invalid name format, should be <name.im.shm>\n");
            free(nameCopy);
            return DAO_ERROR;
        }

    }
    else
    {
        daoError("invalid location, should be /tmp/<optional>/<name.im.shm>\n");
        free(nameCopy);
        return DAO_ERROR;
    }

	
	// Remove pre-existing semaphores if any
    if(image->md[0].sem != NBsem)
    {
        // Close existing semaphores ...
        daoInfo("Closing semaphore\n");
        for(s=0; s < image->md[0].sem; s++)
        {
            sem_close(image->semptr[s]);
        }
        image->md[0].sem = 0;

        daoInfo("Remove associated files\n");
		// ... and remove associated files
        for(s1=NBsem; s1<100; s1++)
        {
            sprintf(fname, "/dev/shm/sem.%s_sem%02ld", localName, s1);
            daoDebug("removing %s\n", fname);
            remove(fname);
        }
        daoInfo("Done\n");
        //free(image->semptr);
        image->semptr = NULL;
    }

   
    if(image->md[0].sem == 0)
    {
        if(image->semptr!=NULL)
            free(image->semptr);

        image->md[0].sem = NBsem;
        daoInfo("malloc semptr %d entries\n", image->md[0].sem);
        image->semptr = (sem_t**) malloc(sizeof(sem_t**)*image->md[0].sem);


        for(s=0; s<NBsem; s++)
        {
            sprintf(shmSemName, "%s_sem%02ld", localName, s);
            if ((image->semptr[s] = sem_open(shmSemName, 0, 0644, 0))== SEM_FAILED) {
                if ((image->semptr[s] = sem_open(shmSemName, O_CREAT, 0644, 1)) == SEM_FAILED) {
                    perror("semaphore initilization\n");
                }
                else
                {
                    sem_init(image->semptr[s], 1, 0);
                }
            }
        }
    }
    free(nameCopy);
    return(0);
}




/*
 * Create SHM
 */
int_fast8_t daoShmImageCreate(IMAGE *image, const char *name, long naxis, 
                              uint32_t *size, uint8_t datatype, int shared, int NBkw)
{
    daoTrace("\n");

    char extracted_name[100];
    if (extract_name(name, extracted_name))
    {
        // CB size = 0 for now
        ImageStreamIO_createIm(image, extracted_name, naxis, size, datatype, shared, NBkw, 0);
    }
    else 
    {
        daoError("Wrong file name, ensure it is using the /milk/shm/<name>.im.shm\n");
        return DAO_ERROR;
    }
    return DAO_SUCCESS;

//    long i;//,ii;
//    long nelement;
//    char shmSemName[256];
//    size_t sharedsize = 0; // shared memory size in bytes
//    int shmFd; // shared memory file descriptor
//    char shmName[256];
//    int result;
//    IMAGE_METADATA *map=NULL;
//    char *mapv; // pointed cast in bytes
//    int kw;
////    char comment[80];
////    char kname[16];
//    nelement = 1;
//    for(i=0; i<naxis; i++)
//    {
//        nelement*=size[i];
//    }
//
//    
//    // compute total size to be allocated
//    if(shared==1)
//    {
//        char * nameCopy = (char *)malloc(strlen(name)*sizeof(char));
//        strcpy(nameCopy, name);
//        char *token = strtok(nameCopy, "/");
//
//        char *semFName = NULL;
//        char *localName = NULL;
//
//        while (token != NULL)
//        {
//            semFName = token;
//            token = strtok(NULL, "/");
//        }
//
//        if (semFName != NULL)
//        {
//            daoInfo("shm name = %s\n", semFName);
//            localName = strtok(semFName, ".");
//            if (localName != NULL)
//            {
//                daoInfo("local name: %s\n", localName);
//            }
//            else
//            {
//                daoError("invalid name format, should be <name.im.shm>\n");
//                free(nameCopy);
//                return DAO_ERROR;
//            }
//        }
//        else
//        {
//            daoError("invalid location, should be /tmp/<optional>/<name.im.shm>\n");
//            free(nameCopy);
//            return DAO_ERROR;
//        }
//
//        // create semlog
//        daoInfo("Creation %s_semlog\n", semFName);
//        sprintf(shmSemName, "%s_semlog", semFName);
//        remove(shmSemName);
//        image->semlog = NULL;
//
//        if ((image->semlog = sem_open(shmSemName, O_CREAT, 0644, 1)) == SEM_FAILED)
//        {
//            perror("semaphore creation / initilization");}
//        else
//        {
//            sem_init(image->semlog, 1, 0);
//        }
//
//        sharedsize = sizeof(IMAGE_METADATA);
//
//        if(datatype == _DATATYPE_UINT8)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_UINT8;
//        }
//        if(datatype == _DATATYPE_INT8)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_INT8;
//        }
//        if(datatype == _DATATYPE_UINT16)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_UINT16;
//        }
//        if(datatype == _DATATYPE_INT16)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_INT16;
//        }
//        if(datatype == _DATATYPE_INT32)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_INT32;
//        }
//        if(datatype == _DATATYPE_UINT32)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_UINT32;
//        }
//        if(datatype == _DATATYPE_INT64)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_INT64;
//        }
//        if(datatype == _DATATYPE_UINT64)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_UINT64;
//        }
//        if(datatype == _DATATYPE_FLOAT)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_FLOAT;
//        }
//        if(datatype == _DATATYPE_DOUBLE)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_DOUBLE;
//        }
//        if(datatype == _DATATYPE_COMPLEX_FLOAT)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_COMPLEX_FLOAT;
//        }
//        if(datatype == _DATATYPE_COMPLEX_DOUBLE)
//        {
//            sharedsize += nelement*SIZEOF_DATATYPE_COMPLEX_DOUBLE;
//        }
//
//        sharedsize += NBkw*sizeof(IMAGE_KEYWORD);
//
//
//        //sprintf(shmName, "%s/%s.im.shm", SHAREDMEMDIR, name);
//        sprintf(shmName, "%s", name);
//        shmFd = open(shmName, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);
//
//        if (shmFd == -1) 
//        {
//            daoError("Error opening file (%s) for writing\n", shmName);
//            exit(0);
//        }
//
//
//
//
//        image->shmfd = shmFd;
//        image->memsize = sharedsize;
//
//        result = lseek(shmFd, sharedsize-1, SEEK_SET);
//        if (result == -1) 
//        {
//            close(shmFd);
//            daoError("Error calling lseek() to 'stretch' the file\n");
//            exit(0);
//        }
//
//        result = write(shmFd, "", 1);
//        if (result != 1) 
//        {
//            close(shmFd);
//            perror("Error writing last byte of the file");
//            exit(0);
//        }
//
//        map = (IMAGE_METADATA*) mmap(0, sharedsize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
//        if (map == MAP_FAILED) 
//        {
//            close(shmFd);
//            perror("Error mmapping the file");
//            exit(0);
//        }
//
//        image->md = (IMAGE_METADATA*) map;
//        image->md[0].shared = 1;
//        image->md[0].sem = 0;
//        free(nameCopy);
//    }
//    else
//    {
//        image->md = (IMAGE_METADATA*) malloc(sizeof(IMAGE_METADATA));
//        image->md[0].shared = 0;
//        if(NBkw>0)
//        {
//            image->kw = (IMAGE_KEYWORD*) malloc(sizeof(IMAGE_KEYWORD)*NBkw);
//        }
//        else
//        {
//            image->kw = NULL;
//        }
//    }
//
//    image->md[0].datatype = datatype;
//    image->md[0].naxis = naxis;
//
//    strcpy(image->name, name); // local name
//    strcpy(image->md[0].name, name);
//    for(i=0; i<naxis; i++)
//    {
//        image->md[0].size[i] = size[i];
//    }
//    image->md[0].NBkw = NBkw;
//
//
//    if(datatype == _DATATYPE_UINT8)
//    {
//        if(shared==1)
//        {
//			mapv = (char*) map;
//            mapv += sizeof(IMAGE_METADATA);
//            image->array.UI8 = (uint8_t*) (mapv);
//            memset(image->array.UI8, '\0', nelement*sizeof(uint8_t));
//            mapv += sizeof(uint8_t)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//		}
//        else
//        {
//            image->array.UI8 = (uint8_t*) calloc ((size_t) nelement, sizeof(uint8_t));
//        }
//
//
//        if(image->array.UI8 == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//                fprintf(stderr,"x%ld", (long) size[i]);
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(uint8_t));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//
//    if(datatype == _DATATYPE_INT8)
//    {
//        if(shared==1)
//        {
//			mapv = (char*) map;
//            mapv += sizeof(IMAGE_METADATA);
//            image->array.SI8 = (int8_t*) (mapv);
//            memset(image->array.SI8, '\0', nelement*sizeof(int8_t));
//            mapv += sizeof(int8_t)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//		}
//        else
//            image->array.SI8 = (int8_t*) calloc ((size_t) nelement, sizeof(int8_t));
//
//
//        if(image->array.SI8 == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//                fprintf(stderr,"x%ld", (long) size[i]);
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(int8_t));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//
//
//
//    if(datatype == _DATATYPE_UINT16)
//    {
//        if(shared==1)
//        {
//            mapv = (char*) map;
//            mapv += sizeof(IMAGE_METADATA);
//            image->array.UI16 = (uint16_t*) (mapv);
//            memset(image->array.UI16, '\0', nelement*sizeof(uint16_t));
//            mapv += sizeof(uint16_t)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//        }
//        else
//        {
//            image->array.UI16 = (uint16_t*) calloc ((size_t) nelement, sizeof(uint16_t));
//        }
//
//        if(image->array.UI16 == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//                fprintf(stderr,"x%ld", (long) size[i]);
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement, 1.0/1024/1024*nelement*sizeof(uint16_t));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//
//    if(datatype == _DATATYPE_INT16)
//    {
//        if(shared==1)
//        {
//            mapv = (char*) map;
//            mapv += sizeof(IMAGE_METADATA);
//            image->array.SI16 = (int16_t*) (mapv);
//            memset(image->array.SI16, '\0', nelement*sizeof(int16_t));
//            mapv += sizeof(int16_t)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//        }
//        else
//        {
//            image->array.SI16 = (int16_t*) calloc ((size_t) nelement, sizeof(int16_t));
//        }
//
//        if(image->array.SI16 == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//            {
//                fprintf(stderr,"x%ld", (long) size[i]);
//            }
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement, 1.0/1024/1024*nelement*sizeof(int16_t));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//
//
//    if(datatype == _DATATYPE_UINT32)
//    {
//        if(shared==1)
//        {
//			mapv = (char*) map;
//            mapv += sizeof(IMAGE_METADATA);
//            image->array.UI32 = (uint32_t*) (mapv);
//            memset(image->array.UI32, '\0', nelement*sizeof(uint32_t));
//            mapv += sizeof(uint32_t)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//        }
//        else
//        {
//            image->array.UI32 = (uint32_t*) calloc ((size_t) nelement, sizeof(uint32_t));
//        }
//
//        if(image->array.UI32 == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//            {
//                fprintf(stderr,"x%ld", (long) size[i]);
//            }
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(uint32_t));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//    
//
//
//    if(datatype == _DATATYPE_INT32)
//    {
//        if(shared==1)
//        {
//			mapv = (char*) map;
//            mapv += sizeof(IMAGE_METADATA);
//            image->array.SI32 = (int32_t*) (mapv);
//            memset(image->array.SI32, '\0', nelement*sizeof(int32_t));
//            mapv += sizeof(int32_t)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//        }
//        else
//        {
//            image->array.SI32 = (int32_t*) calloc ((size_t) nelement, sizeof(int32_t));
//        }
//
//        if(image->array.SI32 == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//            {
//                fprintf(stderr,"x%ld", (long) size[i]);
//            }
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(int32_t));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//    
//    
//    
//    if(datatype == _DATATYPE_UINT64)
//    {
//        if(shared==1)
//        {
//			mapv = (char*) map;
//            mapv += sizeof(IMAGE_METADATA);
//            image->array.UI64 = (uint64_t*) (mapv);
//            memset(image->array.UI64, '\0', nelement*sizeof(uint64_t));
//            mapv += sizeof(uint64_t)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//        }
//        else
//        {
//            image->array.UI64 = (uint64_t*) calloc ((size_t) nelement, sizeof(uint64_t));
//        }
//
//        if(image->array.SI64 == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//            {
//                fprintf(stderr,"x%ld", (long) size[i]);
//            }
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(uint64_t));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//    
//    if(datatype == _DATATYPE_INT64)
//    {
//        if(shared==1)
//        {
//			mapv = (char*) map;
//            mapv += sizeof(IMAGE_METADATA);
//            image->array.SI64 = (int64_t*) (mapv);
//            memset(image->array.SI64, '\0', nelement*sizeof(int64_t));
//            mapv += sizeof(int64_t)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//        }
//        else
//        {
//            image->array.SI64 = (int64_t*) calloc ((size_t) nelement, sizeof(int64_t));
//        }
//
//        if(image->array.SI64 == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//            {
//                fprintf(stderr,"x%ld", (long) size[i]);
//            }
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(int64_t));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//
//
//    if(datatype == _DATATYPE_FLOAT)	
//    {
//        if(shared==1)
//        {
//            mapv = (char*) map;
//            mapv += sizeof(IMAGE_METADATA);
//            image->array.F = (float*) (mapv);
//            memset(image->array.F, '\0', nelement*sizeof(float));
//            mapv += sizeof(float)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//        }
//        else
//        {
//            image->array.F = (float*) calloc ((size_t) nelement, sizeof(float));
//        }
//
//        if(image->array.F == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//            {
//                fprintf(stderr,"x%ld", (long) size[i]);
//            }
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(float));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//    
//    if(datatype == _DATATYPE_DOUBLE)
//    {
//        if(shared==1)
//        {
//			mapv = (char*) map;
//			mapv += sizeof(IMAGE_METADATA);
//			image->array.D = (double*) (mapv);
//            memset(image->array.D, '\0', nelement*sizeof(double));
//            mapv += sizeof(double)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//        }
//        else
//        {
//            image->array.D = (double*) calloc ((size_t) nelement, sizeof(double));
//        }
//  
//        if(image->array.D == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//            {
//                fprintf(stderr,"x%ld", (long) size[i]);
//            }
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(double));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//    
//    if(datatype == _DATATYPE_COMPLEX_FLOAT)
//    {
//        if(shared==1)
//        {
//			mapv = (char*) map;
//			mapv += sizeof(IMAGE_METADATA);
//			image->array.CF = (complex_float*) (mapv);			
//            memset(image->array.CF, '\0', nelement*sizeof(complex_float));
//            mapv += sizeof(complex_float)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//        }
//        else
//        {
//            image->array.CF = (complex_float*) calloc ((size_t) nelement, sizeof(complex_float));
//        }
//
//        if(image->array.CF == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//            {
//                fprintf(stderr,"x%ld", (long) size[i]);
//            }
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(complex_float));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//    
//    if(datatype == _DATATYPE_COMPLEX_DOUBLE)
//    {
//        if(shared==1)
//        {
//			mapv = (char*) map;
//			mapv += sizeof(IMAGE_METADATA);
//			image->array.CD = (complex_double*) (mapv);			
//            memset(image->array.CD, '\0', nelement*sizeof(complex_double));
//            mapv += sizeof(complex_double)*nelement;
//            image->kw = (IMAGE_KEYWORD*) (mapv);
//        }
//        else
//        {
//            image->array.CD = (complex_double*) calloc ((size_t) nelement,sizeof(complex_double));
//        }
//
//        if(image->array.CD == NULL)
//        {
//            daoError("memory allocation failed\n");
//            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
//            fprintf(stderr,"Image name = %s\n",name);
//            fprintf(stderr,"Image size = ");
//            fprintf(stderr,"%ld", (long) size[0]);
//            for(i=1; i<naxis; i++)
//            {
//                fprintf(stderr,"x%ld", (long) size[i]);
//            }
//            fprintf(stderr,"\n");
//            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(complex_double));
//            fprintf(stderr," %c[%d;m",(char) 27, 0);
//            exit(0);
//        }
//    }
//
//    clock_gettime(CLOCK_REALTIME, &image->md[0].lastaccesstime);
//    image->md[0].creationtime = image->md[0].lastaccesstime;
//    image->md[0].write = 0;
//    image->md[0].cnt0 = 0;
//    image->md[0].cnt1 = 0;
//    image->md[0].nelement = nelement;
//
//    if(shared==1)
//    {
//        daoInfo("Creating Semaphores\n");
//        daoImageCreateSem(image, 10); // by default, create 10 semaphores
//        daoInfo("Semaphores created\n");
//    }
//    else
//    {
//		image->md[0].sem = 0; // no semaphores
//    }
//
//    // initialize keywords
//    for(kw=0; kw<image->md[0].NBkw; kw++)
//    {
//        image->kw[kw].type = 'N';
//    }
//    return(0);
}

int_fast8_t daoShmCombineShm2Shm(IMAGE **imageCube, IMAGE *image, int nbChannel, int nbVal)
{
    daoTrace("\n");
    int semval = 0;
    int ss;
    int pp;
    int k;
    image->md[0].write = 1;
    
    // check type and use proper array
    if (image->md[0].datatype == _DATATYPE_UINT8)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.UI8[pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.UI8[pp] += imageCube[k][0].array.UI8[pp];
            }
        }
    }
    else if (image->md[0].datatype == _DATATYPE_INT8)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.SI8[pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.SI8[pp] += imageCube[k][0].array.SI8[pp];
            }
        }
    }
    else if (image->md[0].datatype == _DATATYPE_UINT16)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.UI16[pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.UI16[pp] += imageCube[k][0].array.UI16[pp];
            }
        }
    }
    else if (image->md[0].datatype == _DATATYPE_INT16)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.SI16[pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.SI16[pp] += imageCube[k][0].array.SI16[pp];
            }
        }
    }
    else if (image->md[0].datatype == _DATATYPE_UINT32)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.UI32[pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.UI32[pp] += imageCube[k][0].array.UI32[pp];
            }
        }
    }
    else if (image->md[0].datatype == _DATATYPE_INT32)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.SI32[pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.SI32[pp] += imageCube[k][0].array.SI32[pp];
            }
        }
    }
    else if (image->md[0].datatype == _DATATYPE_UINT64)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.UI64[pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.UI64[pp] += imageCube[k][0].array.UI64[pp];
            }
        }
    }
    else if (image->md[0].datatype == _DATATYPE_INT64)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.SI64[pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.SI64[pp] += imageCube[k][0].array.SI64[pp];
            }
        }
    }
    else if (image->md[0].datatype == _DATATYPE_FLOAT)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.F[pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.F[pp] += imageCube[k][0].array.F[pp];
            }
        }
    }
    else if (image->md[0].datatype == _DATATYPE_DOUBLE)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.D[pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.D[pp] += imageCube[k][0].array.D[pp];
            }
        }
    }


    for(ss = 0; ss < image->md[0].sem; ss++)
    {
        sem_getvalue(image->semptr[ss], &semval);
        if(semval < SEMAPHORE_MAXVAL )
            sem_post(image->semptr[ss]);
    }

    if(image->semlog != NULL)
    {
        sem_getvalue(image->semlog, &semval);
        if(semval < SEMAPHORE_MAXVAL)
        {
            sem_post(image->semlog);
        }
    }

    image->md[0].write = 0;
    image->md[0].cnt0++;
    clock_gettime(CLOCK_REALTIME, &image->md[0].atime);

    return DAO_SUCCESS;
}

/**
 * @brief Wait for new data in SHM
 * 
 * @param image 
 * @return uint_fast64_t 
 */
int_fast8_t daoShmWaitForSemaphore(IMAGE *image, int32_t semNb)
{
    daoTrace("\n");
    // Wait for new image
    sem_wait(image->semptr[semNb]);
    return DAO_SUCCESS;
}

/**
 * @brief Wait for new data in SHM by spining on SHM ocunter
 * 
 * @param image 
 * @return uint_fast64_t 
 */
int_fast8_t daoShmWaitForCounter(IMAGE *image)
{
    daoTrace("\n");
    uint_fast64_t counter = image->md[0].cnt0; 
    struct timespec req, rem;
    req.tv_sec = 0;          // Seconds
    req.tv_nsec = 0; // Nanoseconds
    while (image->md[0].cnt0 <= counter)
    {
        // Spin
        if (nanosleep(&req, &rem) < 0) 
        {
            printf("Nanosleep interrupted\n");
            return 1;
        }
    }
    return DAO_SUCCESS;
}

/**
 * @brief Retreive the SHM counter
 * 
 * @param image 
 * @return uint_fast64_t 
 */
uint_fast64_t daoShmGetCounter(IMAGE *image)
{
    daoTrace("\n");
    return image->md[0].cnt0;
}

/**
 * @brief Read current SHM content
 * 
 * @param image 
 * @return uint_fast64_t 
 */
//void * daoShmGetData(IMAGE *image)
//{
//    daoTrace("\n");
//    // check type and use proper array
//    if (image->md[0].datatype == _DATATYPE_UINT8)
//    {
//                image[0].array.UI8[pp] += imageCube[k][0].array.UI8[pp];
//    }
//    else if (image->md[0].datatype == _DATATYPE_INT8)
//    {
//                image[0].array.SI8[pp] += imageCube[k][0].array.SI8[pp];
//    }
//    else if (image->md[0].datatype == _DATATYPE_UINT16)
//    {
//                image[0].array.UI16[pp] += imageCube[k][0].array.UI16[pp];
//    }
//    else if (image->md[0].datatype == _DATATYPE_INT16)
//    {
//                image[0].array.SI16[pp] += imageCube[k][0].array.SI16[pp];
//    }
//    else if (image->md[0].datatype == _DATATYPE_UINT32)
//    {
//                image[0].array.UI32[pp] += imageCube[k][0].array.UI32[pp];
//    }
//    else if (image->md[0].datatype == _DATATYPE_INT32)
//    {
//                image[0].array.SI32[pp] += imageCube[k][0].array.SI32[pp];
//    }
//    else if (image->md[0].datatype == _DATATYPE_UINT64)
//    {
//                image[0].array.UI64[pp] += imageCube[k][0].array.UI64[pp];
//    }
//    else if (image->md[0].datatype == _DATATYPE_INT64)
//    {
//                image[0].array.SI64[pp] += imageCube[k][0].array.SI64[pp];
//    }
//    else if (image->md[0].datatype == _DATATYPE_FLOAT)
//    {
//                image[0].array.F[pp] += imageCube[k][0].array.F[pp];
//    }
//    else if (image->md[0].datatype == _DATATYPE_DOUBLE)
//    {
//                image[0].array.D[pp] += imageCube[k][0].array.D[pp];
//    }
//    return DAO_SUCCESS;
//}
