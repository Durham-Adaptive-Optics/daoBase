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

#include "daoBase.h"
static int current_log_level = DEFAULT_LOG_LEVEL;
/**
 * Return a time stamp string with microsecond precision
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