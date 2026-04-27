/*****************************************************************************
  DAO project
  RTC library funciton
  S.Cetre
 *****************************************************************************/

/*==========================================================================*/

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0

#define PATH_SEPARATOR "/\\"

// number of seconds between January 1st 1601 (Windows epoch) and January 1st 1970 (Unix epoch)
#define WIN_UNIX_EPOCH_GAP 11644473600

static int clock_gettime(int clk_id, struct timespec *t)
{
	FILETIME currentTime; // stores time in 100-nanosecond intervals
	GetSystemTimePreciseAsFileTime(&currentTime);
	int64_t hundredNanos = (int64_t)currentTime.dwLowDateTime
						 | (int64_t)currentTime.dwHighDateTime << 32;
	hundredNanos -= (WIN_UNIX_EPOCH_GAP * 10000000);
	t->tv_sec = hundredNanos / 10000000;
	t->tv_nsec = (hundredNanos % 10000000) * 100;
	return 0;
}

#else
#include <unistd.h>
#include <semaphore.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <gsl/gsl_blas.h>
#define PATH_SEPARATOR "/"
#endif

#if defined(__linux__)
#include <omp.h>
#endif

#ifdef __APPLE__
#include <stdatomic.h>
#endif

// #ifdef __MACH__
// #include <mach/mach_time.h>
// #define CLOCK_REALTIME 0
// #define CLOCK_MONOTONIC 0
// static int clock_gettime(int clk_id, struct mach_timespec *t)
// {
//     mach_timebase_info_data_t timebase;
//     mach_timebase_info(&timebase);
//     uint64_t time;
//     time = mach_absolute_time();
//     double nseconds = ((double)time * (double)timebase.numer)/((double)timebase.denom);
//     double seconds = ((double)time * (double)timebase.numer)/((double)timebase.denom * 1e9);
//     t->tv_sec = seconds;
//     t->tv_nsec = nseconds;
//     return 0;
// }
// #else

// #endif

#include "dao.h"
static int current_log_level = DEFAULT_LOG_LEVEL;

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

/*
 * Get the current log level.
 */
int daoGetLogLevel() 
{
    return daoLogLevel;
}

// SHM
int NBIMAGES = 10;

//int daoFormatTest(int test1, int test2, char* teststr) {
//	daoTrace("\n");
//	daoInfo("This is info for %d", test1);
//	daoError("This is an error with the string %s", teststr);
//	daoWarning("This is a warning with test2 - %d", test2);
//	return 0;
//}

#ifdef _WIN32

/**
 * Create a security descriptor equivalent to the provided UNIX mode.
 */

SECURITY_ATTRIBUTES* daoCreateWindowsSecurityAttrs(int mode, PACL *dacl)
{
	PACL newDacl = NULL;
	PSID currentUser = NULL;
	SECURITY_ATTRIBUTES *sa = NULL;
	SID everyoneSid;
	PSECURITY_DESCRIPTOR sd = NULL;
	PTOKEN_USER user = NULL;
	DWORD sidSize = SECURITY_MAX_SID_SIZE;
	DWORD result;
	EXPLICIT_ACCESS_ ea[2];
	DWORD size = 0;
	HANDLE token = NULL;
	TOKEN_INFORMATION_CLASS tokenClass = TokenUser;
	
	int success = 0;
	
	do {
		// First, get the SID for "Everyone"
		if (CreateWellKnownSid(WinWorldSid, NULL, &everyoneSid, &sidSize) == FALSE) {
			daoDebug("Could not create SID for 'Everyone'.\n");
			break;
		}
		
		// Now get the token for the current user
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_READ | TOKEN_QUERY, &token)) {
			daoDebug("Could not get handle to access token.\n");
			break;
		} else
			daoDebug("Got handle to access token.\n");
		
		if (!GetTokenInformation(token, tokenClass, NULL, 0, &size)) {
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
				daoDebug("Error in GetTokenInformation1. %d\n", GetLastError());
				break;
			}
		} else
			daoDebug("Buffer for token group is OK.\n");
		
		user = (PTOKEN_USER)malloc((size_t)size);
		if (!user) {
			daoDebug("Malloc error for user token\n");
			break;
		}
		
		if (!GetTokenInformation(token, tokenClass, user, size, &size)) {
			daoDebug("Error in GetTokenInformation.\n");
			break;
		} else
			daoDebug("Got token information for TokenUser.\n");

		DWORD sidLen = GetLengthSid(user->User.Sid);
		currentUser = malloc((size_t)sidLen);
		CopySid(sidLen, currentUser, user->User.Sid);
		
		// Now set up "explicit access" details
		memset(&ea, 0, 2 * sizeof(EXPLICIT_ACCESSA));
		ZeroMemory(&ea, 2 * sizeof(EXPLICIT_ACCESSA));
		
		ea[0].grfAccessPermissions = ACCESS_SYSTEM_SECURITY | READ_CONTROL | WRITE_DAC | GENERIC_ALL | SYNCHRONIZE;
		ea[0].grfAccessMode = GRANT_ACCESS;
		ea[0].grfInheritance = NO_INHERITANCE;
		ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ea[0].Trustee.ptstrName = (LPTSTR)(currentUser);

		ea[1].grfAccessPermissions = ACCESS_SYSTEM_SECURITY | READ_CONTROL;
		ea[1].grfAccessMode = GRANT_ACCESS;
		ea[1].grfInheritance = NO_INHERITANCE;
		ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ea[1].Trustee.ptstrName = (LPTSTR)(&everyoneSid);
		
		int numAclEntries = 1;
		
		if (mode == 0644) {
			numAclEntries = 2;
		} else if (mode == 0600) {
			numAclEntries = 1;
		} else {
			daoDebug("Unsupported mode: %o", mode);
			break;
		}
		
		result = SetEntriesInAcl(numAclEntries, ea, NULL, &newDacl);
		if (ERROR_SUCCESS != result) {
			daoDebug("Error in SetEntriesInAcl: %u", result);
			break;
		}
		
		sd = (PSECURITY_DESCRIPTOR*)malloc(SECURITY_DESCRIPTOR_MIN_LENGTH);
		if (sd == NULL) {
			daoDebug("Malloc error for security descriptor\n");
			break;
		}
		
		if (!InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION)) {
			daoDebug("Could not initialize security descriptor\n");
			break;
		}
		
		if (!SetSecurityDescriptorDacl(sd, TRUE, newDacl, FALSE)) {
			daoDebug("Error in SetSecurityDescriptorDacl\n");
			break;
		}
		
		sa = (SECURITY_ATTRIBUTES*)malloc(sizeof(SECURITY_ATTRIBUTES));
		if (sa == NULL) {
			daoDebug("Malloc error for security attributes.\n");
			break;
		}
		
		sa->nLength = sizeof(SECURITY_ATTRIBUTES);
		sa->lpSecurityDescriptor = sd;
		sa->bInheritHandle = FALSE;
		success = 1;
	} while (0);

	if (!token)
		CloseHandle(token);
	
	if (!user)
		free(user);

	if (!sa) {
		if (!newDacl)
			free(newDacl);
		if (!sd)
			free(sd);
	}

	*dacl = newDacl;

	return sa;
}

/**
 * Free the allocated security descriptor
 */

void daoDestroyWindowsSecurityAttrs(SECURITY_ATTRIBUTES *sa, PACL dacl)
{
	free(dacl);
	free(sa->lpSecurityDescriptor);
	free(sa);
}

#endif

/**
 * Extract image from a shared memory
 */
//int_fast8_t daoShmShm2Img(const char *name, char *prefix, IMAGE *image)
int_fast8_t daoShmShm2Img(const char *name, IMAGE *image)
{
    daoTrace("\n");

    char shmName[256];
    IMAGE_METADATA *map;
    char *mapv;
    uint8_t atype;
    int kw;
    char shmSemName[256];
    int sOK;
    long snb;
    long s;

    uint32_t fifo_size;

#ifdef _WIN32
    HANDLE shmFd; // shared memory file handle
	HANDLE shmFm; // shared memory file mapping object
	HANDLE stest;
	WCHAR wideShmName[256];
	WCHAR wideShmSemName[256];
#else
    int shmFd;
    struct stat file_stat;
    sem_t *stest;
#endif

    int rval = DAO_ERROR;
    sprintf(shmName, "%s", name);
    //sprintf(shmName, "%s/%s%s.im.shm", SHAREDMEMDIR, prefix, name);

#ifdef _WIN32
    MultiByteToWideChar(CP_UTF8, 0, shmName, -1, wideShmName, 256);
	shmFd = CreateFileW(wideShmName, GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL, OPEN_EXISTING, FILE_ATTRIBUTE_TEMPORARY, NULL);
	if (shmFd == INVALID_HANDLE_VALUE)
	{
		image->used = 0;
		daoError("Cannot import shared memory file %s \n", name);
		rval = DAO_ERROR;
	}
#else
    shmFd = open(shmName, O_RDWR);
    if(shmFd==-1)
    {
        image->used = 0;
        daoError("Cannot import shared memory file %s \n", name);
        rval = DAO_ERROR;
    }
#endif
    else
    {
        rval = DAO_SUCCESS; // we assume by default success

#ifdef _WIN32
        DWORD fileSizeHigh, fileSizeLow;
		fileSizeLow = GetFileSize(shmFd, &fileSizeHigh);
		
		// passing 0 for size means "use file's current size"
		shmFm = CreateFileMapping(shmFd, NULL, PAGE_READWRITE, 0, 0, NULL);
		if (shmFm == NULL) {
			CloseHandle(shmFd);
			daoError("Error creating file mapping (%s)\n", shmName);
            rval = DAO_ERROR;
            exit(0);
		}
		
		map = (IMAGE_METADATA*) MapViewOfFile(shmFm, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		if (map == NULL) {
			CloseHandle(shmFd);
			CloseHandle(shmFm);
			daoError("Error creating view of file mapping (%s)\n", shmName);
            rval = DAO_ERROR;
            exit(0);
		}
		
		image->memsize = (int64_t)fileSizeLow | ((int64_t)fileSizeHigh << 32);
		image->shmfd = shmFd;
		image->shmfm = shmFm;
		image->md = map;

#else
		
        fstat(shmFd, &file_stat);
#ifdef __APPLE__
            daoDebug("File %s size: %lld\n", shmName, file_stat.st_size);
#else
            daoDebug("File %s size: %ld\n", shmName, file_stat.st_size);
#endif
        map = (IMAGE_METADATA*) mmap(0, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
        if (map == MAP_FAILED) 
        {
            close(shmFd);
            perror("Error mmapping the file");
            rval = DAO_ERROR;
            exit(0);
        }

        image->memsize = file_stat.st_size;
        image->shmfd = shmFd;
        image->md = map;
        //        image->md[0].sem = 0;

#endif
        atype = image->md[0].atype;
        image->md[0].shared = 1;

        daoDebug("image size = %ld %ld\n", (long) image->md[0].size[0], (long) image->md[0].size[1]);
        fflush(stdout);
        // some verification
//        if(image->md[0].size[0]*image->md[0].size[1]>10000000000)
//        {
//            daoDebug("IMAGE \"%s\" SEEMS BIG... ABORTING\n", name);
//            rval = DAO_ERROR;
//            exit(0);
//        }
        if(image->md[0].size[0]<1)
        {
            daoError("IMAGE \"%s\" AXIS SIZE < 1... ABORTING\n", name);
            rval = DAO_ERROR;
            exit(0);
        }
        if(image->md[0].size[1]<1)
        {
            daoError("IMAGE \"%s\" AXIS SIZE < 1... ABORTING\n", name);
            rval = DAO_ERROR;
            exit(0);
        }

        /* Get the size of the FIFO */

        fifo_size = image->md[0].fifo_size;

        mapv = (char*) map;
        mapv += fifo_size * sizeof(IMAGE_METADATA);

        daoDebug("atype = %d\n", (int) atype);
        fflush(stdout);

        if(atype == _DATATYPE_UINT8)
        {
            daoDebug("atype = UINT8\n");
            image->array.UI8 = (uint8_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT8 * fifo_size * image->md[0].nelement;
        }
        if(atype == _DATATYPE_INT8)
        {
            daoDebug("atype = INT8\n");
            image->array.SI8 = (int8_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT8 * fifo_size * image->md[0].nelement;
        }
        if(atype == _DATATYPE_UINT16)
        {
            daoDebug("atype = UINT16\n");
            image->array.UI16 = (uint16_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT16 * fifo_size * image->md[0].nelement;
        }
        if(atype == _DATATYPE_INT16)
        {
            daoDebug("atype = INT16\n");
            image->array.SI16 = (int16_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT16 * fifo_size * image->md[0].nelement;
        }
        if(atype == _DATATYPE_UINT32)
        {
            daoDebug("atype = UINT32\n");
            image->array.UI32 = (uint32_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT32 * fifo_size * image->md[0].nelement;
        }
        if(atype == _DATATYPE_INT32)
        {
            daoDebug("atype = INT32\n");
            image->array.SI32 = (int32_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT32 * fifo_size * image->md[0].nelement;
        }
        if(atype == _DATATYPE_UINT64)
        {
            daoDebug("atype = UINT64\n");
            image->array.UI64 = (uint64_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT64 * fifo_size * image->md[0].nelement;
        }
        if(atype == _DATATYPE_INT64)
        {
            daoDebug("atype = INT64\n");
            image->array.SI64 = (int64_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT64 * fifo_size * image->md[0].nelement;
        }
        if(atype == _DATATYPE_FLOAT)
        {
            daoDebug("atype = FLOAT\n");
            image->array.F = (float*) mapv;
            mapv += SIZEOF_DATATYPE_FLOAT * fifo_size * image->md[0].nelement;
        }
        if(atype == _DATATYPE_DOUBLE)
        {
            daoDebug("atype = DOUBLE\n");
            image->array.D = (double*) mapv;
            mapv += SIZEOF_DATATYPE_DOUBLE * fifo_size * image->md[0].nelement;
        }
        if(atype == _DATATYPE_COMPLEX_FLOAT)
        {
            daoDebug("atype = COMPLEX_FLOAT\n");
            image->array.CF = (complex_float*) mapv;
            mapv += SIZEOF_DATATYPE_COMPLEX_FLOAT * fifo_size * image->md[0].nelement;
        }
        if(atype == _DATATYPE_COMPLEX_DOUBLE)
        {
            daoDebug("atype = COMPLEX_DOUBLE\n");
            image->array.CD = (complex_double*) mapv;
            mapv += SIZEOF_DATATYPE_COMPLEX_DOUBLE * fifo_size * image->md[0].nelement;
        }
        daoDebug("%ld keywords\n", (long) image->md[0].NBkw);
        fflush(stdout);

        image->kw = (IMAGE_KEYWORD*) (mapv);
        for(kw=0; kw<image->md[0].NBkw; kw++)
        {
            if(image->kw[kw].type == 'L')
                #if defined(_WIN32) || defined(__APPLE__)
                daoDebug("%d  %s %lld %s\n", kw, image->kw[kw].name, image->kw[kw].value.numl, image->kw[kw].comment);
				#else
                daoDebug("%d  %s %ld %s\n", kw, image->kw[kw].name, image->kw[kw].value.numl, image->kw[kw].comment);
				#endif
            if(image->kw[kw].type == 'D')
                daoDebug("%d  %s %lf %s\n", kw, image->kw[kw].name, image->kw[kw].value.numf, image->kw[kw].comment);
            if(image->kw[kw].type == 'S')
                daoDebug("%d  %s %s %s\n", kw, image->kw[kw].name, image->kw[kw].value.valstr, image->kw[kw].comment);
        }

        mapv += sizeof(IMAGE_KEYWORD)*image->md[0].NBkw;
        strcpy(image->name, name);
        // to get rid off warning
        char * nameCopy = (char *)malloc((strlen(name)+1)*sizeof(char));
        strcpy(nameCopy, name);
        char *token = strtok(nameCopy, PATH_SEPARATOR);

        char *semFName = NULL;
        char *localName = NULL;

        while (token != NULL)
        {
            semFName = token;
            token = strtok(NULL, PATH_SEPARATOR);
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

#ifdef _WIN32
        // create semaphores one-by-one, and stop when we get one that does NOT exist
		PACL dacl;
		SECURITY_ATTRIBUTES *saShm = daoCreateWindowsSecurityAttrs(0644, &dacl);
		if (!saShm) {
			daoError("Could not create security descriptor - using default\n");
			free(nameCopy);
			return DAO_ERROR;
		}
		
		snb = 0;
		sOK = 1;
		while(sOK==1)
		{
            sprintf(shmSemName, "Local\\DAO_%s_sem%02ld", localName, snb);
			MultiByteToWideChar(CP_UTF8, 0, shmSemName, -1, wideShmSemName, 256);
            daoDebug("semaphore %s\n", shmSemName);
			stest = CreateSemaphoreW(saShm, 0, 1, wideShmSemName);
            if(stest == NULL)
            {
				daoDebug("no handle returned %d\n", snb);
                sOK = 0;
            }
			else if (GetLastError() != ERROR_ALREADY_EXISTS)
			{
				daoDebug("ERROR              %d\n", snb);
				CloseHandle(stest);
				sOK = 0;
			}
            else
            {
				daoDebug("exists             %d\n", snb);
                CloseHandle(stest);
                snb++;
            }
		}
        daoDebug("%ld semaphores detected  (image->md[0].sem = %d)\n", snb, (int) image->md[0].sem);
        //        image->md[0].sem = snb;
        image->semptr = (HANDLE*) malloc(sizeof(HANDLE) * image->md[0].sem);
        for(s=0; s<snb; s++)
        {
            sprintf(shmSemName, "Local\\DAO_%s_sem%02ld", localName, s);
			MultiByteToWideChar(CP_UTF8, 0, shmSemName, -1, wideShmSemName, 256);
            if ((image->semptr[s] = CreateSemaphoreW(saShm, 0, 1, wideShmSemName)) == NULL) 
            {
                daoError("could not open semaphore %s\n", shmSemName);
            }
			daoDebug("Handle %p %d\n", image->semptr[s], snb);
        }
		daoDebug("%d found\n", snb);
		
        sprintf(shmSemName, "Local\\DAO_%s_semlog", localName);
		MultiByteToWideChar(CP_UTF8, 0, shmSemName, -1, wideShmSemName, 256);
        if ((image->semlog = CreateSemaphoreW(saShm, 0, 1, wideShmSemName)) == NULL) 
        {
            daoWarning("could not open semaphore %s\n", shmSemName);
        }

		daoDestroyWindowsSecurityAttrs(saShm, dacl);
#else
        // looking for semaphores

		snb = 0;
        sOK = 1;
        while(sOK==1)
        {
            sprintf(shmSemName, "%s_sem%02ld", localName, snb);
            daoDebug("semaphore %s\n", shmSemName);
            if((stest = sem_open(shmSemName, 0, 0644, 0))== SEM_FAILED)
            {
                sOK = 0;
            }
            else
            {
                sem_close(stest);
                snb++;
            }
        }
        daoDebug("%ld semaphores detected  (image->md[0].sem = %d)\n", snb, (int) image->md[0].sem);
        //        image->md[0].sem = snb;
        image->semptr = (sem_t**) malloc(sizeof(sem_t*) * image->md[0].sem);
        for(s=0; s<snb; s++)
        {
            sprintf(shmSemName, "%s_sem%02ld", localName, s);
            if ((image->semptr[s] = sem_open(shmSemName, 0, 0644, 0))== SEM_FAILED) 
            {
                daoError("could not open semaphore %s\n", shmSemName);
            }
        }

        sprintf(shmSemName, "%s_semlog", localName);
        if ((image->semlog = sem_open(shmSemName, 0, 0644, 0))== SEM_FAILED) 
        {
            daoWarning("could not open semaphore %s\n", shmSemName);
        }
#endif
        free(nameCopy);

        // Set up FIFO tail

        uint32_t tmp32;
        uint64_t tmp64;
        daoShmResetTail(image, &tmp32, &tmp64);
    }
    return(rval);
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
#ifdef _WIN32
    daoDebug("sizeof(image) = %lld\n", sizeof(IMAGE));
#else
    daoDebug("sizeof(image) = %ld\n", sizeof(IMAGE));
#endif
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

    volatile IMAGE_METADATA *vol_md = (volatile IMAGE_METADATA *)image->md;

    uint32_t last_written = vol_md[0].fifo_last_written;
    uint32_t writing_idx = (last_written + 1) % (image->md[0].fifo_size);

    uint64_t fifo_writing_offset = image->md[0].nelement * (uint64_t)writing_idx;

    vol_md[writing_idx].write = 1;

    if (image->md[writing_idx].atype == _DATATYPE_UINT8)
        memcpy(&image->array.UI8[fifo_writing_offset], (unsigned char *)im, nbVal*sizeof(unsigned char)); 
    else if (image->md[writing_idx].atype == _DATATYPE_INT8)
        memcpy(&image->array.SI8[fifo_writing_offset], (char *)im, nbVal*sizeof(char));       
    else if (image->md[writing_idx].atype == _DATATYPE_UINT16)
        memcpy(&image->array.UI16[fifo_writing_offset], (unsigned short *)im, nbVal*sizeof(unsigned short));
    else if (image->md[writing_idx].atype == _DATATYPE_INT16)
        memcpy(&image->array.SI16[fifo_writing_offset], (short *)im, nbVal*sizeof(short));
    else if (image->md[writing_idx].atype == _DATATYPE_UINT32)
        memcpy(&image->array.UI32[fifo_writing_offset], (unsigned int *)im, nbVal*sizeof(unsigned int));
    else if (image->md[writing_idx].atype == _DATATYPE_INT32)
        memcpy(&image->array.SI32[fifo_writing_offset], (int *)im, nbVal*sizeof(int));
    else if (image->md[writing_idx].atype == _DATATYPE_UINT64)
        memcpy(&image->array.UI64[fifo_writing_offset], (unsigned long *)im, nbVal*sizeof(unsigned long));
    else if (image->md[writing_idx].atype == _DATATYPE_INT64)
        memcpy(&image->array.SI64[fifo_writing_offset], (long *)im, nbVal*sizeof(long));
    else if (image->md[writing_idx].atype == _DATATYPE_FLOAT)
        memcpy(&image->array.F[fifo_writing_offset], (float *)im, nbVal*sizeof(float));
    else if (image->md[writing_idx].atype == _DATATYPE_DOUBLE)
        memcpy(&image->array.D[fifo_writing_offset], (double *)im, nbVal*sizeof(double));
    else if (image->md[writing_idx].atype == _DATATYPE_COMPLEX_FLOAT)
        memcpy(&image->array.CF[fifo_writing_offset], (complex_float *)im, nbVal*sizeof(complex_float));
    else if (image->md[writing_idx].atype == _DATATYPE_COMPLEX_DOUBLE)
        memcpy(&image->array.CD[fifo_writing_offset], (complex_double *)im, nbVal*sizeof(complex_double));

    daoShmImagePart2ShmFinalize(image);

    return DAO_SUCCESS;
}

/*
 * The image is send to the shared memory.
 */
int_fast8_t daoShmImage2ShmQuiet(void *im, uint32_t nbVal, IMAGE *image) 
{
    daoTrace("\n");

    volatile IMAGE_METADATA *vol_md = (volatile IMAGE_METADATA *)image->md;

    uint32_t last_written = vol_md[0].fifo_last_written;
    uint32_t writing_idx = (last_written + 1) % (image->md[0].fifo_size);

    uint64_t fifo_writing_offset = image->md[0].nelement * (uint64_t)writing_idx;

    image->md[writing_idx].write = 1;

    if (image->md[0].atype == _DATATYPE_UINT8)
        memcpy(&image->array.UI8[fifo_writing_offset], (unsigned char *)im, nbVal*sizeof(unsigned char)); 
    else if (image->md[0].atype == _DATATYPE_INT8)
        memcpy(&image->array.SI8[fifo_writing_offset], (char *)im, nbVal*sizeof(char));       
    else if (image->md[0].atype == _DATATYPE_UINT16)
        memcpy(&image->array.UI16[fifo_writing_offset], (unsigned short *)im, nbVal*sizeof(unsigned short));
    else if (image->md[0].atype == _DATATYPE_INT16)
        memcpy(&image->array.SI16[fifo_writing_offset], (short *)im, nbVal*sizeof(short));
    else if (image->md[0].atype == _DATATYPE_UINT32)
        memcpy(&image->array.UI32[fifo_writing_offset], (unsigned int *)im, nbVal*sizeof(unsigned int));
    else if (image->md[0].atype == _DATATYPE_INT32)
        memcpy(&image->array.SI32[fifo_writing_offset], (int *)im, nbVal*sizeof(int));
    else if (image->md[0].atype == _DATATYPE_UINT64)
        memcpy(&image->array.UI64[fifo_writing_offset], (unsigned long *)im, nbVal*sizeof(unsigned long));
    else if (image->md[0].atype == _DATATYPE_INT64)
        memcpy(&image->array.SI64[fifo_writing_offset], (long *)im, nbVal*sizeof(long));
    else if (image->md[0].atype == _DATATYPE_FLOAT)
        memcpy(&image->array.F[fifo_writing_offset], (float *)im, nbVal*sizeof(float));
    else if (image->md[0].atype == _DATATYPE_DOUBLE)
        memcpy(&image->array.D[fifo_writing_offset], (double *)im, nbVal*sizeof(double));
    else if (image->md[0].atype == _DATATYPE_COMPLEX_FLOAT)
        memcpy(&image->array.CF[fifo_writing_offset], (complex_float *)im, nbVal*sizeof(complex_float));
    else if (image->md[0].atype == _DATATYPE_COMPLEX_DOUBLE)
        memcpy(&image->array.CD[fifo_writing_offset], (complex_double *)im, nbVal*sizeof(complex_double));
	
    image->md[writing_idx].write = 0;

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

    volatile IMAGE_METADATA *vol_md = (volatile IMAGE_METADATA *)image->md;

    uint32_t last_written = vol_md[0].fifo_last_written;
    uint32_t writing_idx = (last_written + 1) % (image->md[0].fifo_size);

    uint64_t fifo_writing_offset = image->md[0].nelement * (uint64_t)writing_idx;

    uint64_t adjusted_position = (uint64_t)position + fifo_writing_offset;

    image->md[writing_idx].write = 1;

    if (image->md[0].atype == _DATATYPE_UINT8)
        memcpy(&image->array.UI8[adjusted_position], (unsigned char *)im, nbVal*sizeof(unsigned char)); 
    else if (image->md[0].atype == _DATATYPE_INT8)
        memcpy(&image->array.SI8[adjusted_position], (char *)im, nbVal*sizeof(char));       
    else if (image->md[0].atype == _DATATYPE_UINT16)
        memcpy(&image->array.UI16[adjusted_position], (unsigned short *)im, nbVal*sizeof(unsigned short));
    else if (image->md[0].atype == _DATATYPE_INT16)
        memcpy(&image->array.SI16[adjusted_position], (short *)im, nbVal*sizeof(short));
    else if (image->md[0].atype == _DATATYPE_UINT32)
        memcpy(&image->array.UI32[adjusted_position], (unsigned int *)im, nbVal*sizeof(unsigned int));
    else if (image->md[0].atype == _DATATYPE_INT32)
        memcpy(&image->array.SI32[adjusted_position], (int *)im, nbVal*sizeof(int));
    else if (image->md[0].atype == _DATATYPE_UINT64)
        memcpy(&image->array.UI64[adjusted_position], (unsigned long *)im, nbVal*sizeof(unsigned long));
    else if (image->md[0].atype == _DATATYPE_INT64)
        memcpy(&image->array.SI64[adjusted_position], (long *)im, nbVal*sizeof(long));
    else if (image->md[0].atype == _DATATYPE_FLOAT)
        memcpy(&image->array.F[adjusted_position], (float *)im, nbVal*sizeof(float));
    else if (image->md[0].atype == _DATATYPE_DOUBLE)
        memcpy(&image->array.D[adjusted_position], (double *)im, nbVal*sizeof(double));
    else if (image->md[0].atype == _DATATYPE_COMPLEX_FLOAT)
        memcpy(&image->array.CF[adjusted_position], (complex_float *)im, nbVal*sizeof(complex_float));
    else if (image->md[0].atype == _DATATYPE_COMPLEX_DOUBLE)
        memcpy(&image->array.CD[adjusted_position], (complex_double *)im, nbVal*sizeof(complex_double));

    image->md[writing_idx].lastPos = position;
    image->md[writing_idx].lastNb = nbVal;
    image->md[writing_idx].packetNb = packetId;
    image->md[writing_idx].packetTotal = packetTotal;
    image->md[writing_idx].lastNbArray[packetId] = frameNumber;
    image->md[writing_idx].write = 0;

    return DAO_SUCCESS;
}

/*
 * The image has beed sent to the shared memory.
 * Release of semaphore since it is a part write
 */
int_fast8_t daoShmImagePart2ShmFinalize(IMAGE *image) 
{
    daoTrace("\n");

    volatile IMAGE_METADATA *vol_md = (volatile IMAGE_METADATA *)image->md;

    uint32_t last_written = vol_md[0].fifo_last_written;
    uint32_t writing_idx = (last_written + 1) % (image->md[0].fifo_size);

    image->md[writing_idx].write = 0;

    image->md[0].fifo_last_written = writing_idx;

    daoShmTimestampShm(image);
    daoSemPostAll(image);

    if(image->semlog != NULL)
    {
        daoSemLogPost(image);
    }
	 

    return DAO_SUCCESS;
}

/**
 * @brief 
 * 
 * @param image 
 * @param NBsem 
 * @return int_fast8_t 
 */
int_fast8_t daoImageCreateSem(IMAGE *image, long NBsem)
{
    daoTrace("\n");
    char shmSemName[256];
    long s;
//    int r;
//    char command[256];
//    int semfile[100];

#ifdef _WIN32
	WCHAR wideShmSemName[256];
	PACL dacl;
#else
	char fname[256];
    long s1;
#endif

	//printf("%p\n", image);
	//printf("%p\n", &(image->md[0]));
	//printf("%p\n", &(image->md[0].name));
	//printf("%s\n", image->md[0].name);
	//printf("%p\n", &strlen);

    // to get rid off warning
    char * nameCopy = (char *)malloc((strlen(image->md[0].name)+1)*sizeof(char));
    strcpy(nameCopy, image->md[0].name);
	
    char *token = strtok(nameCopy, PATH_SEPARATOR);

    char *semFName = NULL;
    char *localName = NULL; 

    while (token != NULL) 
    {
        semFName = token;
        token = strtok(NULL, PATH_SEPARATOR);
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
#ifdef _WIN32
    SECURITY_ATTRIBUTES *saShm = daoCreateWindowsSecurityAttrs(0644, &dacl);
	if (!saShm) {
		daoError("Could not create semaphore security attributes\n");
		free(nameCopy);
		return DAO_ERROR;
	}
	
    if(image->md[0].sem != NBsem)
    {
        // Close existing semaphores ...
        daoInfo("Closing semaphore\n");
        for(s=0; s < image->md[0].sem; s++)
        {
            CloseHandle(image->semptr[s]);
        }
        image->md[0].sem = 0;

        daoInfo("Done\n");
        //free(image->semptr);
        image->semptr = NULL;
    }
#else
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
#endif


    if(image->md[0].sem == 0)
    {
        if(image->semptr!=NULL)
            free(image->semptr);

        image->md[0].sem = (uint16_t)NBsem;
        daoInfo("malloc semptr %d entries\n", image->md[0].sem);

#ifdef _WIN32
        image->semptr = (HANDLE*) malloc(sizeof(HANDLE*)*image->md[0].sem);
#else
        image->semptr = (sem_t**) malloc(sizeof(sem_t**)*image->md[0].sem);
#endif
        
        for(s=0; s<NBsem; s++)
        {
#ifdef _WIN32
            sprintf(shmSemName, "Local\\DAO_%s_sem%02ld", localName, s);
			MultiByteToWideChar(CP_UTF8, 0, shmSemName, -1, wideShmSemName, 256);
			
			if (!(image->semptr[s] = CreateSemaphoreW(saShm, 0, 1, wideShmSemName))) {
				DWORD last_error = GetLastError();
				fprintf(stderr, "semaphore initialization error: %d\n", GetLastError());
			}
			ReleaseSemaphore(image->semptr[s], 1, NULL);

#else
            sprintf(shmSemName, "%s_sem%02ld", localName, s);
            if ((image->semptr[s] = sem_open(shmSemName, 0, 0644, 0))== SEM_FAILED) 
            {
                if ((image->semptr[s] = sem_open(shmSemName, O_CREAT, 0644, 1)) == SEM_FAILED) 
                {
                    perror("semaphore initilization\n");
                }
                else
                {
                    sem_init(image->semptr[s], 1, 0);
                }
            }
#endif
        }
    }

#ifdef _WIN32
    daoDestroyWindowsSecurityAttrs(saShm, dacl);
#endif

    free(nameCopy);
    return(0);
}




/*
 * Create SHM
 */
int_fast8_t daoShmImageCreate_FIFO(IMAGE *image, const char *name, long naxis, 
                              uint32_t *size, uint8_t atype, int shared, int NBkw, uint32_t fifo_size)
{
    daoTrace("\n");
    long i;//,ii;
    long nelement;
    struct timespec timenow;
    char shmSemName[256];
    size_t sharedsize = 0; // shared memory size in bytes
    char shmName[256];
    IMAGE_METADATA *map=NULL;
    char *mapv; // pointed cast in bytes
    int kw;
//    char comment[80];
//    char kname[16];
    nelement = 1;

#ifdef _WIN32
    WCHAR wideShmSemName[256];
	WCHAR wideShmName[256];
	HANDLE shmFd; // shared memory file handle
	HANDLE shmFm; // shared memory file mapping object
#else
    int shmFd; // shared memory file descriptor
    int result;
#endif
	
    for(i=0; i<naxis; i++)
    {
        nelement*=size[i];
    }

    
    // compute total size to be allocated
    if(shared==1)
    {
        char * nameCopy = (char *)malloc((strlen(name)+1)*sizeof(char));
        strcpy(nameCopy, name);
        char *token = strtok(nameCopy, PATH_SEPARATOR);

        char *semFName = NULL;
        char *localName = NULL;

        while (token != NULL)
        {
            semFName = token;
            token = strtok(NULL, PATH_SEPARATOR);
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

        // create semlog
#ifdef _WIN32
        daoInfo("Creation Local\\DAO_%s_semlog\n", semFName);
        sprintf(shmSemName, "Local\\DAO_%s_semlog", semFName);
        image->semlog = NULL;
		
		MultiByteToWideChar(CP_UTF8, 0, shmSemName, -1, wideShmSemName, 256);
		
		PACL daclSem;
		SECURITY_ATTRIBUTES *saSem = daoCreateWindowsSecurityAttrs(0600, &daclSem);
		if (!saSem) {
			daoError("Error creating security attributes for semaphore\n");
			free(nameCopy);
			return DAO_ERROR;
		}
		
		if (!(image->semlog = CreateSemaphoreW(saSem, 0, 1, wideShmSemName))) {
			DWORD last_error = GetLastError();
			fprintf(stderr, "semaphore initialization error: %d\n", GetLastError());
		}
		ReleaseSemaphore(image->semlog, 1, NULL);
		daoDestroyWindowsSecurityAttrs(saSem, daclSem);

#else
        daoInfo("Creation %s_semlog\n", semFName);
        sprintf(shmSemName, "%s_semlog", semFName);
        remove(shmSemName);
        image->semlog = NULL;

        if ((image->semlog = sem_open(shmSemName, O_CREAT, 0644, 1)) == SEM_FAILED)
        {
            perror("semaphore creation / initilization");}
        else
        {
            sem_init(image->semlog, 1, 0);
        }
#endif

        /* First, determine the space requirements for storing all copies of the image metadata */
        sharedsize = fifo_size * sizeof(IMAGE_METADATA);

        /* Now add the space requirements for the buffers themselves */
        if(atype == _DATATYPE_UINT8)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_UINT8;
        }
        if(atype == _DATATYPE_INT8)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_INT8;
        }
        if(atype == _DATATYPE_UINT16)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_UINT16;
        }
        if(atype == _DATATYPE_INT16)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_INT16;
        }
        if(atype == _DATATYPE_INT32)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_INT32;
        }
        if(atype == _DATATYPE_UINT32)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_UINT32;
        }
        if(atype == _DATATYPE_INT64)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_INT64;
        }
        if(atype == _DATATYPE_UINT64)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_UINT64;
        }
        if(atype == _DATATYPE_FLOAT)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_FLOAT;
        }
        if(atype == _DATATYPE_DOUBLE)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_DOUBLE;
        }
        if(atype == _DATATYPE_COMPLEX_FLOAT)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_COMPLEX_FLOAT;
        }
        if(atype == _DATATYPE_COMPLEX_DOUBLE)
        {
            sharedsize += fifo_size * nelement * SIZEOF_DATATYPE_COMPLEX_DOUBLE;
        }

        /* And finally, set aside space for the keywords */
        sharedsize += NBkw * sizeof(IMAGE_KEYWORD);

#ifdef _WIN32
        sprintf(shmName, "%s", name);
		MultiByteToWideChar(CP_UTF8, 0, shmName, -1, wideShmName, 256);
		
		PACL daclFile;
		SECURITY_ATTRIBUTES *saFile = daoCreateWindowsSecurityAttrs(0600, &daclFile);
		if (!saFile) {
			daoError("Error creating security attributes for file\n");
			exit(0);
		}

		// Use OPEN_ALWAYS instead of CREATE_ALWAYS.
		// CREATE_ALWAYS truncates the file on open, but Windows returns
		// ERROR_USER_MAPPED_FILE (1224) if any other process currently has the
		// file memory-mapped (e.g. a reader is still connected).
		// OPEN_ALWAYS creates the file if absent, or opens it without
		// truncating if it already exists.  We zero the mapped region
		// explicitly below to get the same clean-slate effect.
		shmFd = CreateFileW(wideShmName, GENERIC_READ | GENERIC_WRITE,
							FILE_SHARE_READ | FILE_SHARE_WRITE,
							saFile, OPEN_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);

		if (shmFd == INVALID_HANDLE_VALUE)
		{
			DWORD error_num = GetLastError();
			daoError("Error opening file (%s) for writing, error %d\n", shmName, error_num);
			exit(0);
		}
		
		// Create file mapping handle. Specifying the full sharedsize here
		// will extend the file if it is smaller than required (e.g. first
		// run, or size changed).
		shmFm = CreateFileMapping(shmFd, NULL, PAGE_READWRITE,
								  (DWORD)(sharedsize >> 32), (DWORD)sharedsize, NULL);
		if (shmFm == NULL) {
			CloseHandle(shmFd);
			daoError("Error creating file mapping (%s)\n", shmName);
			exit(0);
		}
		
		image->shmfd = shmFd;
		image->shmfm = shmFm;
		image->memsize = sharedsize;
		
		// Now map file into memory - "map" will be our pointer
		map = (IMAGE_METADATA*) MapViewOfFile(shmFm, FILE_MAP_ALL_ACCESS, 0, 0, sharedsize);
		if (map == NULL) {
			CloseHandle(shmFd);
			CloseHandle(shmFm);
			daoError("Error creating view of file mapping (%s)\n", shmName);
		}

		// Zero the region so stale data from a previous writer session is
		// cleared (equivalent to the truncation that CREATE_ALWAYS would do).
		memset(map, 0, sharedsize);

#else
        //sprintf(shmName, "%s/%s.im.shm", SHAREDMEMDIR, name);
        sprintf(shmName, "%s", name);
        shmFd = open(shmName, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);

        if (shmFd == -1) 
        {
            daoError("Error opening file (%s) for writing\n", shmName);
            exit(0);
        }


        image->shmfd = shmFd;
        image->memsize = sharedsize;

        result = lseek(shmFd, sharedsize-1, SEEK_SET);
        if (result == -1) 
        {
            close(shmFd);
            daoError("Error calling lseek() to 'stretch' the file\n");
            exit(0);
        }

        result = write(shmFd, "", 1);
        if (result != 1) 
        {
            close(shmFd);
            perror("Error writing last byte of the file");
            exit(0);
        }

        map = (IMAGE_METADATA*) mmap(0, sharedsize, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
        if (map == MAP_FAILED) 
        {
            close(shmFd);
            perror("Error mmapping the file");
            exit(0);
        }
#endif
	
        image->md = (IMAGE_METADATA*) map;
        image->md[0].fifo_size = fifo_size;

        for (uint32_t fifo_idx = 0; fifo_idx < fifo_size; ++fifo_idx)
        {
            image->md[fifo_idx].shared = 1;
            image->md[fifo_idx].sem = 0;
        }

        free(nameCopy);

#ifdef _WIN32
        daoDestroyWindowsSecurityAttrs(saFile, daclFile);
#endif
    }
    else
    {
        image->md = (IMAGE_METADATA*) malloc(sizeof(IMAGE_METADATA) * fifo_size);
        image->md[0].fifo_size = fifo_size;

        for (uint32_t fifo_idx = 0; fifo_idx < fifo_size; ++fifo_idx)
        {
            image->md[fifo_idx].shared = 0;
        }

        if(NBkw>0)
        {
            image->kw = (IMAGE_KEYWORD*) malloc(sizeof(IMAGE_KEYWORD)*NBkw);
        }
        else
        {
            image->kw = NULL;
        }
    }

    strcpy(image->name, name); // local name
    for (uint32_t fifo_idx = 0; fifo_idx < fifo_size; ++fifo_idx)
    {
        image->md[fifo_idx].atype = atype;
        image->md[fifo_idx].naxis = (uint8_t)naxis;

        strcpy(image->md[fifo_idx].name, name);

        for(i=0; i<naxis; i++)
        {
            image->md[fifo_idx].size[i] = size[i];
        }
        image->md[fifo_idx].NBkw = NBkw;
    }


    if(atype == _DATATYPE_UINT8)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA) * fifo_size;
            image->array.UI8 = (uint8_t*) (mapv);
            memset(image->array.UI8, '\0', nelement*sizeof(uint8_t));
            mapv += sizeof(uint8_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
		}
        else
        {
            image->array.UI8 = (uint8_t*) calloc ((size_t) nelement * (size_t) fifo_size, sizeof(uint8_t));
        }


        if(image->array.UI8 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(uint8_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }

    if(atype == _DATATYPE_INT8)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA) * fifo_size;
            image->array.SI8 = (int8_t*) (mapv);
            memset(image->array.SI8, '\0', nelement*sizeof(int8_t));
            mapv += sizeof(int8_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
		}
        else
            image->array.SI8 = (int8_t*) calloc ((size_t) nelement * (size_t) fifo_size, sizeof(int8_t));


        if(image->array.SI8 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(int8_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }



    if(atype == _DATATYPE_UINT16)
    {
        if(shared==1)
        {
            mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA) * fifo_size;
            image->array.UI16 = (uint16_t*) (mapv);
            memset(image->array.UI16, '\0', nelement*sizeof(uint16_t));
            mapv += sizeof(uint16_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.UI16 = (uint16_t*) calloc ((size_t) nelement * (size_t) fifo_size, sizeof(uint16_t));
        }

        if(image->array.UI16 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(uint16_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }

    if(atype == _DATATYPE_INT16)
    {
        if(shared==1)
        {
            mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA) * fifo_size;
            image->array.SI16 = (int16_t*) (mapv);
            memset(image->array.SI16, '\0', nelement*sizeof(int16_t));
            mapv += sizeof(int16_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.SI16 = (int16_t*) calloc ((size_t) nelement * (size_t) fifo_size, sizeof(int16_t));
        }

        if(image->array.SI16 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
            {
                fprintf(stderr,"x%ld", (long) size[i]);
            }
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(int16_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }


    if(atype == _DATATYPE_UINT32)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA) * fifo_size;
            image->array.UI32 = (uint32_t*) (mapv);
            memset(image->array.UI32, '\0', nelement*sizeof(uint32_t));
            mapv += sizeof(uint32_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.UI32 = (uint32_t*) calloc ((size_t) nelement * (size_t) fifo_size, sizeof(uint32_t));
        }

        if(image->array.UI32 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
            {
                fprintf(stderr,"x%ld", (long) size[i]);
            }
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(uint32_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    


    if(atype == _DATATYPE_INT32)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA) * fifo_size;
            image->array.SI32 = (int32_t*) (mapv);
            memset(image->array.SI32, '\0', nelement*sizeof(int32_t));
            mapv += sizeof(int32_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.SI32 = (int32_t*) calloc ((size_t) nelement * (size_t) fifo_size, sizeof(int32_t));
        }

        if(image->array.SI32 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
            {
                fprintf(stderr,"x%ld", (long) size[i]);
            }
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(int32_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    
    
    
    if(atype == _DATATYPE_UINT64)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA) * fifo_size;
            image->array.UI64 = (uint64_t*) (mapv);
            memset(image->array.UI64, '\0', nelement*sizeof(uint64_t));
            mapv += sizeof(uint64_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.UI64 = (uint64_t*) calloc ((size_t) nelement * (size_t) fifo_size, sizeof(uint64_t));
        }

        if(image->array.SI64 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
            {
                fprintf(stderr,"x%ld", (long) size[i]);
            }
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(uint64_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    
    if(atype == _DATATYPE_INT64)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA) * fifo_size;
            image->array.SI64 = (int64_t*) (mapv);
            memset(image->array.SI64, '\0', nelement*sizeof(int64_t));
            mapv += sizeof(int64_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.SI64 = (int64_t*) calloc ((size_t) nelement * (size_t) fifo_size, sizeof(int64_t));
        }

        if(image->array.SI64 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
            {
                fprintf(stderr,"x%ld", (long) size[i]);
            }
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(int64_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }


    if(atype == _DATATYPE_FLOAT)	
    {
        if(shared==1)
        {
            mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA) * fifo_size;
            image->array.F = (float*) (mapv);
            memset(image->array.F, '\0', nelement*sizeof(float));
            mapv += sizeof(float)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.F = (float*) calloc ((size_t) nelement * (size_t) fifo_size, sizeof(float));
        }

        if(image->array.F == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
            {
                fprintf(stderr,"x%ld", (long) size[i]);
            }
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(float));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    
    if(atype == _DATATYPE_DOUBLE)
    {
        if(shared==1)
        {
			mapv = (char*) map;
			mapv += sizeof(IMAGE_METADATA) * fifo_size;
			image->array.D = (double*) (mapv);
            memset(image->array.D, '\0', nelement*sizeof(double));
            mapv += sizeof(double)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.D = (double*) calloc ((size_t) nelement * (size_t) fifo_size, sizeof(double));
        }
  
        if(image->array.D == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
            {
                fprintf(stderr,"x%ld", (long) size[i]);
            }
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(double));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    
    if(atype == _DATATYPE_COMPLEX_FLOAT)
    {
        if(shared==1)
        {
			mapv = (char*) map;
			mapv += sizeof(IMAGE_METADATA) * fifo_size;
			image->array.CF = (complex_float*) (mapv);			
            memset(image->array.CF, '\0', nelement*sizeof(complex_float));
            mapv += sizeof(complex_float)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.CF = (complex_float*) calloc ((size_t) nelement * (size_t) fifo_size, sizeof(complex_float));
        }

        if(image->array.CF == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
            {
                fprintf(stderr,"x%ld", (long) size[i]);
            }
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(complex_float));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    
    if(atype == _DATATYPE_COMPLEX_DOUBLE)
    {
        if(shared==1)
        {
			mapv = (char*) map;
			mapv += sizeof(IMAGE_METADATA) * fifo_size;
			image->array.CD = (complex_double*) (mapv);			
            memset(image->array.CD, '\0', nelement*sizeof(complex_double));
            mapv += sizeof(complex_double)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.CD = (complex_double*) calloc ((size_t) nelement * (size_t) fifo_size,sizeof(complex_double));
        }

        if(image->array.CD == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
            {
                fprintf(stderr,"x%ld", (long) size[i]);
            }
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements * %ld buffers = %f Mb\n",
                (long) nelement, (long) fifo_size, 1.0/1024/1024*nelement*fifo_size*sizeof(complex_double));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }

    clock_gettime(CLOCK_REALTIME, &timenow);

    for (uint32_t fifo_idx = 0; fifo_idx < fifo_size; ++fifo_idx)
    {
        image->md[fifo_idx].last_access = 1.0*timenow.tv_sec + 0.000000001*timenow.tv_nsec;
        image->md[fifo_idx].creation_time = image->md[0].last_access;
        image->md[fifo_idx].write = 0;
        image->md[fifo_idx].cnt0 = 0;
        image->md[fifo_idx].cnt1 = 0;
        image->md[fifo_idx].nelement = nelement;
    }

    if(shared==1)
    {
        daoInfo("Creating Semaphores\n");
        daoImageCreateSem(image, 10); // by default, create 10 semaphores
        daoInfo("Semaphores created\n");

#ifdef __APPLE__
        for (int i = 0; i < image->md[0].sem; i++)
        {
            while (sem_trywait(image->semptr[i]) == 0) 
            {
                // Draining kernel-level semaphore
            }
            atomic_store(&image->md[0].semCounter[i], 0);
        }
        atomic_store(&image->md[0].semLogCounter, 0);
#endif
    }
    else
    {
		image->md[0].sem = 0; // no semaphores
    }

    // set fifo last written position
    image->md[0].fifo_size = fifo_size;
    image->md[0].fifo_last_written = fifo_size - 1;

    // set up FIFO tail

    uint32_t tmp32;
    uint64_t tmp64;
    daoShmResetTail(image, &tmp32, &tmp64);

    // initialize keywords
    for(kw=0; kw<image->md[0].NBkw; kw++)
    {
        image->kw[kw].type = 'N';
    }
    return(0);
}

int_fast8_t daoShmCombineShm2Shm(IMAGE **imageCube, IMAGE *image, int nbChannel, int nbVal)
{
    daoTrace("\n");
    int pp;
    int k;
    uint64_t fifo_reading_offset[DAO_MAX_COMBINE_CHANNELS];

    // Get output image writing position
    volatile IMAGE_METADATA *vol_md = (volatile IMAGE_METADATA *)image->md;

    uint32_t last_written = vol_md[0].fifo_last_written;
    uint32_t writing_idx = (last_written + 1) % (image->md[0].fifo_size);

    uint64_t fifo_writing_offset = image->md[0].nelement * (uint64_t)writing_idx;

    vol_md[writing_idx].write = 1;

    // Fail if we are trying to combine too many channels
    if (nbChannel > DAO_MAX_COMBINE_CHANNELS)
    {
        daoError("Attempted to combine more channels than is supported (%d > %d)\n",
            nbChannel, DAO_MAX_COMBINE_CHANNELS);
        return DAO_ERROR;
    }

    // Get input image reading positions
    for (int k = 0; k < nbChannel; ++k)
    {
        volatile IMAGE_METADATA *reading_md = (volatile IMAGE_METADATA *)imageCube[k]->md;

        uint32_t last_written = reading_md[0].fifo_last_written;
        uint32_t writing_idx = (last_written + 1) % (imageCube[k]->md[0].fifo_size);

        fifo_reading_offset[k] = imageCube[k]->md[0].nelement * (uint64_t)writing_idx;
    }
    
    // check type and use proper array
    if (image->md[0].atype == _DATATYPE_UINT8)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.UI8[fifo_writing_offset + pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.UI8[fifo_writing_offset + pp]
                    += imageCube[k][0].array.UI8[fifo_reading_offset[k] + pp];
            }
        }
    }
    else if (image->md[0].atype == _DATATYPE_INT8)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.SI8[fifo_writing_offset + pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.SI8[fifo_writing_offset + pp]
                    += imageCube[k][0].array.SI8[fifo_reading_offset[k] + pp];
            }
        }
    }
    else if (image->md[0].atype == _DATATYPE_UINT16)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.UI16[fifo_writing_offset + pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.UI16[fifo_writing_offset + pp]
                    += imageCube[k][0].array.UI16[fifo_reading_offset[k] + pp];
            }
        }
    }
    else if (image->md[0].atype == _DATATYPE_INT16)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.SI16[fifo_writing_offset + pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.SI16[fifo_writing_offset + pp]
                    += imageCube[k][0].array.SI16[fifo_reading_offset[k] + pp];
            }
        }
    }
    else if (image->md[0].atype == _DATATYPE_UINT32)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.UI32[fifo_writing_offset + pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.UI32[fifo_writing_offset + pp]
                    += imageCube[k][0].array.UI32[fifo_reading_offset[k] + pp];
            }
        }
    }
    else if (image->md[0].atype == _DATATYPE_INT32)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.SI32[fifo_writing_offset + pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.SI32[fifo_writing_offset + pp]
                    += imageCube[k][0].array.SI32[fifo_reading_offset[k] + pp];
            }
        }
    }
    else if (image->md[0].atype == _DATATYPE_UINT64)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.UI64[fifo_writing_offset + pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.UI64[fifo_writing_offset + pp]
                    += imageCube[k][0].array.UI64[fifo_reading_offset[k] + pp];
            }
        }
    }
    else if (image->md[0].atype == _DATATYPE_INT64)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.SI64[fifo_writing_offset + pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.SI64[fifo_writing_offset + pp]
                    += imageCube[k][0].array.SI64[fifo_reading_offset[k] + pp];
            }
        }
    }
    else if (image->md[0].atype == _DATATYPE_FLOAT)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.F[fifo_writing_offset + pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.F[fifo_writing_offset + pp]
                    += imageCube[k][0].array.F[fifo_reading_offset[k] + pp];
            }
        }
    }
    else if (image->md[0].atype == _DATATYPE_DOUBLE)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.D[fifo_writing_offset + pp] = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.D[fifo_writing_offset + pp]
                    += imageCube[k][0].array.D[fifo_reading_offset[k] + pp];
            }
        }
    }
    else if (image->md[0].atype == _DATATYPE_COMPLEX_FLOAT)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.CF[fifo_writing_offset + pp].re = 0;
            image[0].array.CF[fifo_writing_offset + pp].im = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.CF[fifo_writing_offset + pp].re
                    += imageCube[k][0].array.CF[fifo_reading_offset[k] + pp].re;
                image[0].array.CF[fifo_writing_offset + pp].im
                    += imageCube[k][0].array.CF[fifo_reading_offset[k] + pp].im;
            }
        }
    }
        else if (image->md[0].atype == _DATATYPE_COMPLEX_DOUBLE)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.CD[fifo_writing_offset + pp].re = 0;
            image[0].array.CD[fifo_writing_offset + pp].im = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.CD[fifo_writing_offset + pp].re
                    += imageCube[k][0].array.CD[fifo_reading_offset[k] + pp].re;
                image[0].array.CD[fifo_writing_offset + pp].im
                    += imageCube[k][0].array.CD[fifo_reading_offset[k] + pp].im;
            }
        }
    }
	
    daoShmImagePart2ShmFinalize(image);

    return DAO_SUCCESS;
}

/* Function for compatibility - creates 1-deep DAO SHM */
int_fast8_t daoShmImageCreate(IMAGE *image, const char *name, long naxis, 
                              uint32_t *size, uint8_t atype, int shared, int NBkw)
{
    return daoShmImageCreate_FIFO(image, name, naxis, size, atype, shared, NBkw, 1);
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
#ifdef _WIN32
    // On Windows we use a single short-timeout wait and return DAO_TIMEOUT so
    // that the *Python* caller can loop. If the loop were here in C, Python
    // would never get control back between iterations and KeyboardInterrupt
    // (Ctrl+C) could never fire.
    //
    // If the handle is NULL or invalid (writer hasn't started yet, or the
    // writer exited and its semaphore was destroyed), try to reopen it by
    // name before waiting. Named semaphores in the Local\ namespace are
    // destroyed when the last handle is closed, so the reader must reopen
    // them each time the writer restarts.
    if (!image->semptr || !image->semptr[semNb]) {
        // Build the semaphore name from the image name, same scheme as
        // daoImageCreateSem / daoShmShm2Img.
        char semName[256];
        WCHAR wSemName[256];
        char nameCopy[256];
        strncpy(nameCopy, image->md[0].name, sizeof(nameCopy)-1);
        nameCopy[sizeof(nameCopy)-1] = '\0';
        char *tok = strtok(nameCopy, PATH_SEPARATOR);
        char *last = NULL;
        while (tok) { last = tok; tok = strtok(NULL, PATH_SEPARATOR); }
        if (last) {
            char *localName = strtok(last, ".");
            if (localName) {
                sprintf(semName, "Local\\DAO_%s_sem%02d", localName, (int)semNb);
                MultiByteToWideChar(CP_UTF8, 0, semName, -1, wSemName, 256);
                HANDLE h = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, wSemName);
                if (h && image->semptr) {
                    image->semptr[semNb] = h;
                }
            }
        }
        // Still no handle — writer not running yet, report timeout so caller loops
        if (!image->semptr || !image->semptr[semNb]) {
            return DAO_TIMEOUT;
        }
    }

    DWORD dWaitResult = WaitForSingleObject(image->semptr[semNb], 100);
    if (dWaitResult == WAIT_OBJECT_0) {
        return DAO_SUCCESS;
    } else if (dWaitResult == WAIT_TIMEOUT) {
        return DAO_TIMEOUT;
    } else {
        DWORD err = GetLastError();
        if (err == ERROR_INVALID_HANDLE) {
            // The writer exited and its semaphore was destroyed.
            // Invalidate the handle so the next call will reopen it.
            CloseHandle(image->semptr[semNb]);
            image->semptr[semNb] = NULL;
            return DAO_TIMEOUT;
        }
        daoError("Unexpected result from WaitForSingleObject (Windows system error no. %d).\n", err);
        return DAO_ERROR;
    }
#elif defined(__APPLE__)
    if (sem_wait(image->semptr[semNb]) == 0) 
    {
        // Safely decrement the counter, but only if > 0
        unsigned int val = atomic_load(&image->md[0].semCounter[semNb]);
        while (val > 0)
        {
            if (atomic_compare_exchange_weak(&image->md[0].semCounter[semNb], &val, val - 1))
                break;
            // If CAS fails, val is updated, loop again
        }
    } 
    else
    {
        daoError("sem_wait failed on sem %d: %s\n", semNb, strerror(errno));
        return DAO_ERROR;
    }
#else
    sem_wait(image->semptr[semNb]);
#endif

    return DAO_SUCCESS;
}

/**
 * @brief Wait for new data in SHM with time out
 * 
 * @param image 
 * @return uint_fast64_t 
 * @timeout timespec
 */
int_fast8_t daoShmWaitForSemaphoreTimeout(IMAGE *image, int32_t semNb, const struct timespec * timeout)
{
    daoTrace("\n");
    // Wait for new image
#ifdef _WIN32
    // Convert absolute timespec to milliseconds from now
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    DWORD millis =
        (timeout->tv_sec - now.tv_sec) * 1000 +
        (timeout->tv_nsec - now.tv_nsec) / 1000000;

    if ((int)millis < 0) millis = 0; // prevent underflow

	DWORD dWaitResult = WaitForSingleObject(image->semptr[semNb], millis);
	switch (dWaitResult) {
		case WAIT_OBJECT_0:
			break;
        case WAIT_TIMEOUT:
            return DAO_TIMEOUT;
		default:
			daoError("Unexpected result from WaitForSingleObject (Windows system error no. %d).\n", GetLastError());
			return DAO_ERROR;
	}
#elif defined(__APPLE__)
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);

    uint64_t now_ns = (uint64_t)now.tv_sec * 1e9 + now.tv_nsec;
    uint64_t end_ns = (uint64_t)timeout->tv_sec * 1e9 + timeout->tv_nsec;

    while (now_ns < end_ns)
    {
        unsigned int val = atomic_load(&image->md[0].semCounter[semNb]);
        if (val > 0)
        {
            if (sem_wait(image->semptr[semNb]) == 0)
            {
                atomic_fetch_sub(&image->md[0].semCounter[semNb], 1);
                return DAO_SUCCESS;
            }
            else
            {
                daoError("sem_wait failed: %s\n", strerror(errno));
                return DAO_ERROR;
            }
        }

        // Sleep briefly (e.g., 100 µs)
        struct timespec sleep_ts = {.tv_sec = 0, .tv_nsec = 100000};
        nanosleep(&sleep_ts, NULL);

        clock_gettime(CLOCK_REALTIME, &now);
        now_ns = (uint64_t)now.tv_sec * 1e9 + now.tv_nsec;
    }

    return DAO_TIMEOUT;
#else
    if (sem_timedwait(image->semptr[semNb], timeout) == -1)
    {
        return DAO_TIMEOUT;
    }
#endif

    return DAO_SUCCESS;
}

/**
 * @brief Wait for new data in SHM by spining on SHM counter
 * 
 * @param image 
 * @return int_fast8_t 
 */
int_fast8_t daoShmWaitForCounter(IMAGE *image)
{
    daoTrace("\n");
    volatile IMAGE_METADATA *md = (volatile IMAGE_METADATA *)image->md;

    if (md->fifo_size == 1) // Spin on our only cnt0 for 1-deep images
    {
    #ifdef _WIN32
        volatile uint_fast64_t counter = md->cnt0;
        while (md->cnt0 <= counter)
        {
            // Spin
        }
    #else
        uint_fast64_t counter = image->md[0].cnt0;
        struct timespec req, rem;
        req.tv_sec = 0;          // Seconds
        req.tv_nsec = 0; // Nanoseconds
        while (md->cnt0 <= counter)
        {
            // Spin
            if (nanosleep(&req, &rem) < 0) 
            {
                printf("Nanosleep interrupted\n");
                return DAO_ERROR;
            }
        }
    #endif
        return DAO_SUCCESS;
    }
    else // Spin on writing position for everything else
    {
    #ifdef _WIN32
        volatile uint32_t fifo_position = md->fifo_last_written;
        while (md->fifo_last_written == fifo_position)
        {
            // Spin
        }
    #else
        uint32_t fifo_position = md->fifo_last_written;
        struct timespec req, rem;
        req.tv_sec = 0;          // Seconds
        req.tv_nsec = 0; // Nanoseconds
        while (md->fifo_last_written == fifo_position)
        {
            // Spin
            if (nanosleep(&req, &rem) < 0) 
            {
                printf("Nanosleep interrupted\n");
                return DAO_ERROR;
            }
        }
    #endif
        return DAO_SUCCESS;
    }

}

/**
 * @brief Wait for new data in SHM by spining until the provided SHM counter is reached or exceeded.
 * 
 * @param image 
 * @param targetCnt0 The cnt0 value to spin until. 
 * @return int_fast8_t 
 */
int_fast8_t daoShmWaitForTargetCounter(IMAGE *image, uint64_t targetCnt0)
{
    daoTrace("\n");

    volatile IMAGE_METADATA *md = (volatile IMAGE_METADATA *)image->md;

    #ifdef _WIN32
    if (md->fifo_size == 1) // Spin on our only cnt0 for 1-deep images
    {
        while (md->cnt0 < targetCnt0)
        {
            // Spin with small delay to avoid consuming 100% CPU
            Sleep(0); // Yield the current time slice
        }
    }
    else // Keep checking writing position
    {
        while (1)
        {
            // Check counter of last position
            uint32_t fifo_last_written = md[0].fifo_last_written;
            if (md[fifo_last_written].cnt0 >= targetCnt0)
                break;
            Sleep(0); // Yield the current time slice
        }
    }
#else
    struct timespec req, rem;
    req.tv_sec = 0;          // Seconds
    req.tv_nsec = 0; // Nanoseconds

    if (md->fifo_size == 1) // Spin on our only cnt0 for 1-deep images
    {
        while (md->cnt0 < targetCnt0)
        {
            // Spin
            if (nanosleep(&req, &rem) < 0) 
            {
                printf("Nanosleep interrupted\n");
                return DAO_ERROR;
            }
        }
    }
    else // Keep checking writing position
    {
        while (1)
        {
            // Check counter of last position
            uint32_t fifo_last_written = md[0].fifo_last_written;

            if (md[fifo_last_written].cnt0 >= targetCnt0)
                break;

            // Spin
            if (nanosleep(&req, &rem) < 0) 
            {
                printf("Nanosleep interrupted\n");
                return DAO_ERROR;
            }
        }
    }
#endif

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

    uint32_t fifo_last_written = image->md[0].fifo_last_written;

    return image->md[fifo_last_written].cnt0;
}

/**
 * @brief Get the next segment of this shared memory, or the newest if our tail position has been lapped.
 * 
 * @param image 
 * @param segment_ptr Pointer to start of the next segment's array
 * @param segment_idx Numerical index of the next segment
 * @return int_fast8_t DAO_OVERWRITE if tail has been lapped, otherwise DAO_SUCCESS
 */
int_fast8_t daoShmGetNextSegment(IMAGE *image, void** segment_ptr, uint32_t* segment_idx, uint64_t *segment_cnt0)
{
    // Pseudocode
    // 1. get last_read_idx from IMAGE
    // 3. increment last_read_idx next segment
    // 4. get cnt0 from next segment
    // 5. if next segment_cnt0 != last_read_cnt0 + 1, we have been lapped, so reset tail
    // 6. set segment_ptr and segment_idx with last_read_idx
    // 2. set to last_read_idx 
    // 7. return DAO_OVERWRITE if lapped, DAO_SUCCESS otherwise

    int_fast8_t return_val = DAO_SUCCESS;

    volatile IMAGE *vol_image = (volatile IMAGE *)image;
    volatile IMAGE_METADATA *vol_md = (volatile IMAGE_METADATA *)image->md;

    uint32_t last_read_idx  = vol_image->fifo_last_read;
    uint64_t last_read_cnt0 = vol_image->fifo_last_read_cnt0;

    // 2. increment last_read_idx to next segment
    uint32_t next_segment_idx = (last_read_idx + 1) % vol_md[0].fifo_size;
    uint64_t next_segment_cnt0 = last_read_cnt0 + 1;

    // Return if we haven't got any new data yet
    if ((last_read_idx == vol_md[0].fifo_last_written) && (last_read_cnt0 == vol_md[last_read_idx].cnt0))
    {
        return DAO_NOTREADY;
    }

    if (vol_md[next_segment_idx].cnt0 != next_segment_cnt0)
    { // We have been lapped by the writer. Set our position to the newest segment and signal the overwrite condition
        return_val = DAO_OVERWRITE;
        next_segment_idx = vol_md[0].fifo_last_written;
        next_segment_cnt0 = vol_md[next_segment_idx].cnt0;
    }

    // Set the segment pointer and index
    if (image->md[0].atype == _DATATYPE_UINT8)
        *segment_ptr = &(image->array.UI8[next_segment_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT8)
        *segment_ptr = &(image->array.SI8[next_segment_idx * image->md[0].nelement]);      
    else if (image->md[0].atype == _DATATYPE_UINT16)
        *segment_ptr = &(image->array.UI16[next_segment_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT16)
        *segment_ptr = &(image->array.SI16[next_segment_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_UINT32)
        *segment_ptr = &(image->array.UI32[next_segment_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT32)
        *segment_ptr = &(image->array.SI32[next_segment_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_UINT64)
        *segment_ptr = &(image->array.UI64[next_segment_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT64)
        *segment_ptr = &(image->array.SI64[next_segment_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_FLOAT)
        *segment_ptr = &(image->array.F[next_segment_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_DOUBLE)
        *segment_ptr = &(image->array.D[next_segment_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_COMPLEX_FLOAT)
        *segment_ptr = &(image->array.CF[next_segment_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_COMPLEX_DOUBLE)
        *segment_ptr = &(image->array.CD[next_segment_idx * image->md[0].nelement]);

    *segment_idx = next_segment_idx;
    *segment_cnt0 = next_segment_cnt0;

    // Update the bookkeeping variables for this FIFO tail our IMAGE struct
    vol_image->fifo_last_read = next_segment_idx;
    vol_image->fifo_last_read_cnt0 = next_segment_cnt0;

    return return_val;
}

/**
 * @brief Wait until the next segment is written to the DAO shared memory.
 * 
 * @param image 
 * @return int_fast8_t
 */
int_fast8_t daoShmWaitForNextSegment(IMAGE *image)
{
    return daoShmWaitForTargetCounter(image, image->fifo_last_read_cnt0 + 1);
}

/**
 * @brief Get the specified segment of this shared memory.
 * 
 * @param image 
 * @param segment_ptr Pointer to start of the given segment's array
 * @param segment_idx Numerical index of the segment to get
 * @return int_fast8_t
 */
int_fast8_t daoShmGetArbitrarySegment(IMAGE *image, void** segment_ptr, uint_fast32_t fifo_idx)
{
    uint32_t actual_idx = (uint32_t)(fifo_idx % image->md[0].fifo_size);

    if (image->md[0].atype == _DATATYPE_UINT8)
        *segment_ptr = &(image->array.UI8[actual_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT8)
        *segment_ptr = &(image->array.SI8[actual_idx * image->md[0].nelement]);      
    else if (image->md[0].atype == _DATATYPE_UINT16)
        *segment_ptr = &(image->array.UI16[actual_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT16)
        *segment_ptr = &(image->array.SI16[actual_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_UINT32)
        *segment_ptr = &(image->array.UI32[actual_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT32)
        *segment_ptr = &(image->array.SI32[actual_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_UINT64)
        *segment_ptr = &(image->array.UI64[actual_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT64)
        *segment_ptr = &(image->array.SI64[actual_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_FLOAT)
        *segment_ptr = &(image->array.F[actual_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_DOUBLE)
        *segment_ptr = &(image->array.D[actual_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_COMPLEX_FLOAT)
        *segment_ptr = &(image->array.CF[actual_idx * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_COMPLEX_DOUBLE)
        *segment_ptr = &(image->array.CD[actual_idx * image->md[0].nelement]);

    return DAO_SUCCESS;
}

/**
 * @brief Get the segment of this shared memory which was last written to.
 * 
 * @param image 
 * @param segment_ptr Pointer to start of the newest segment's array
 * @param segment_idx Numerical index of the newest segment
 * @return int_fast8_t
 */
int_fast8_t daoShmGetNewestSegment(IMAGE *image, void** segment_ptr, uint32_t* segment_idx, uint64_t *segment_cnt0)
{
    volatile IMAGE_METADATA *vol_md = (volatile IMAGE_METADATA *)image->md;

    uint32_t last_written = vol_md[0].fifo_last_written;
    uint64_t cnt0 = vol_md[last_written].cnt0;

    if (image->md[0].atype == _DATATYPE_UINT8)
        *segment_ptr = &(image->array.UI8[last_written * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT8)
        *segment_ptr = &(image->array.SI8[last_written * image->md[0].nelement]);      
    else if (image->md[0].atype == _DATATYPE_UINT16)
        *segment_ptr = &(image->array.UI16[last_written * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT16)
        *segment_ptr = &(image->array.SI16[last_written * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_UINT32)
        *segment_ptr = &(image->array.UI32[last_written * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT32)
        *segment_ptr = &(image->array.SI32[last_written * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_UINT64)
        *segment_ptr = &(image->array.UI64[last_written * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_INT64)
        *segment_ptr = &(image->array.SI64[last_written * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_FLOAT)
        *segment_ptr = &(image->array.F[last_written * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_DOUBLE)
        *segment_ptr = &(image->array.D[last_written * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_COMPLEX_FLOAT)
        *segment_ptr = &(image->array.CF[last_written * image->md[0].nelement]);
    else if (image->md[0].atype == _DATATYPE_COMPLEX_DOUBLE)
        *segment_ptr = &(image->array.CD[last_written * image->md[0].nelement]);

    *segment_idx = last_written;
    *segment_cnt0 = cnt0;

    return DAO_SUCCESS;
}

/**
 * @brief Indicates if the segment was overwritten since it was last inspected
 * 
 * @param image 
 * @return int_fast8_t DAO_OVERWRITE if segment was overwritten, DAO_SUCCESS otherwise
 */
int_fast8_t daoShmCheckSegmentOverwrite(IMAGE *image)
{
    // Pseudocode
    // 1. get last read CNT0 from IMAGE
    // 2. get new value of CNT0 for this segment from IMAGE_METADATA
    // 3. Check if new_value == last read, or if write flag is set
    // 4. return accordingly

    int_fast8_t return_val = DAO_SUCCESS;

    volatile IMAGE *vol_image = (volatile IMAGE *)image;
    volatile IMAGE_METADATA *vol_md = (volatile IMAGE_METADATA *)image->md;

    uint32_t last_read_idx = vol_image->fifo_last_read;
    uint64_t last_read_cnt0 = vol_image->fifo_last_read_cnt0;

    if (vol_md[last_read_idx].write == 1 || vol_md[last_read_idx].cnt0 != last_read_cnt0)
    {
        return_val = DAO_OVERWRITE;
    }

    return return_val;
}

/**
 * @brief Reset the reading tail on this SHM image struct to the current newest segment.
 * 
 * @param image 
 * @param segment_idx Segment to which the tail was reset
 * @param segment_cnt0 Current CNT0 of the segment to which the tail was reset
 * @return int_fast8_t
 */
int_fast8_t daoShmResetTail(IMAGE *image, uint32_t* segment_idx, uint64_t *segment_cnt0)
{
    volatile IMAGE_METADATA *vol_md = (volatile IMAGE_METADATA *)image->md;

    uint32_t new_segment_idx = vol_md[0].fifo_last_written;
    uint64_t new_segment_cnt0 = vol_md[new_segment_idx].cnt0;

    *segment_idx = new_segment_idx;
    *segment_cnt0 = new_segment_cnt0;

    image->fifo_last_read = new_segment_idx;
    image->fifo_last_read_cnt0 = new_segment_cnt0;

    return DAO_SUCCESS;
}


/**
 * Clean up shared memory resources
 * 
 * Properly closes file descriptors, unmaps memory, and releases semaphores
 */
int_fast8_t daoShmCloseShm(IMAGE *image)
{
    daoTrace("\n");
    int s;
    
    if (!image) {
        daoWarning("Null image pointer passed to daoShmCloseShm\n");
        return DAO_ERROR;
    }

    // Release all semaphores
    if (image->semptr) {
        for(s = 0; s < image->md[0].sem; s++) {
#ifdef _WIN32
            if (image->semptr[s]) {
                CloseHandle(image->semptr[s]);
                image->semptr[s] = NULL;
            }
#else
            if (image->semptr[s]) {
                sem_close(image->semptr[s]);
                image->semptr[s] = NULL;
            }
#endif
        }
        free(image->semptr);
        image->semptr = NULL;
    }
    // Close log semaphore if it exists
#ifdef _WIN32
    if (image->semlog) {
        CloseHandle(image->semlog);
        image->semlog = NULL;
    }
#else
    if (image->semlog) {
        sem_close(image->semlog);
        image->semlog = NULL;
    }
#endif

    // Free PID arrays if they exist
    if (image->semReadPID) {
        free(image->semReadPID);
        image->semReadPID = NULL;
    }
    
    if (image->semWritePID) {
        free(image->semWritePID);
        image->semWritePID = NULL;
    }

    // Unmap memory and close file descriptor if shared
    if (image->md[0].shared) {
#ifdef _WIN32
        // Windows: Unmap the view and close handles
        if (image->md) {
            UnmapViewOfFile((LPVOID)image->md);
            image->md = NULL;
        }
        if (image->shmfm) {
            CloseHandle(image->shmfm);
            image->shmfm = NULL;
        }
        if (image->shmfd) {
            CloseHandle(image->shmfd);
            image->shmfd = NULL;
        }
#else
        // Unix: Unmap memory and close file descriptor
        if (image->md) {
            munmap(image->md, image->memsize);
            image->md = NULL;
        }
        if (image->shmfd > 0) {
            close(image->shmfd);
            image->shmfd = -1;
        }
#endif
    }

    // Mark image as unused
    image->used = 0;
    
    return DAO_SUCCESS;
}

/**
 * @brief Time stamp the most recently written segment of the image
 * 
 * @param image 
 * @return int_fast8_t 
 */
int_fast8_t daoShmTimestampShm(IMAGE *image)
{
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);

    volatile IMAGE_METADATA *vol_md = (volatile IMAGE_METADATA *)image->md;

    uint32_t fifo_last_written = vol_md[0].fifo_last_written;
    uint32_t fifo_prior_write = (fifo_last_written == 0)
                                ? vol_md[0].fifo_size - 1
                                : fifo_last_written - 1;

    vol_md[fifo_last_written].atime.tsfixed.secondlong = ((int64_t)(1e9 * t.tv_sec) + t.tv_nsec);
    vol_md[fifo_last_written].cnt0 = vol_md[fifo_prior_write].cnt0 + 1;

    return DAO_SUCCESS;
}

/**
 * @brief Post a semaphore
 * 
 * @param image 
 * @param semNb 
 * @return int_fast8_t 
 */
int_fast8_t daoSemPost(IMAGE *image, int32_t semNb)
{
    daoTrace("\n");

#ifdef _WIN32
    ReleaseSemaphore(image->semptr[semNb], 1, NULL);
#elif defined(__APPLE__)
    unsigned int oldval = atomic_load(&image->md[0].semCounter[semNb]);

    while (oldval < SEMAPHORE_MAXVAL)
    {
        // Try to increment semCounter atomically
        if (atomic_compare_exchange_weak(&image->md[0].semCounter[semNb], &oldval, oldval + 1))
        {
            // Only post if we managed to reserve a "slot" in semCounter
            if (sem_post(image->semptr[semNb]) == 0)
            {
                daoDebug("Posted sem %d, new semCounter = %u (pid=%d)\n",
                         semNb, atomic_load(&image->md[0].semCounter[semNb]), getpid());
                return DAO_SUCCESS;
            }
            else
            {
                // Rollback the counter change
                atomic_fetch_sub(&image->md[0].semCounter[semNb], 1);
                daoError("sem_post failed on sem %d: %s\n", semNb, strerror(errno));
                return DAO_ERROR;
            }
        }
        // If compare_exchange failed, oldval is updated to current — loop and try again
    }

    daoDebug("semCounter[%d] already at max value (%d), skipping post\n",
             semNb, SEMAPHORE_MAXVAL);
#else // Linux
    int semval = 0;
    sem_getvalue(image->semptr[semNb], &semval);
    if(semval < SEMAPHORE_MAXVAL )
    {
        sem_post(image->semptr[semNb]);
    }
#endif
    return DAO_SUCCESS;
}
/**
 * @brief Post all semaphore
 * 
 * @param image 
 * @return int_fast8_t 
 */
int_fast8_t daoSemPostAll(IMAGE *image)
{
    daoTrace("\n");
    int ss;
    for(ss = 0; ss < image->md[0].sem; ss++)
    {
        daoSemPost(image, ss);
    }
    return DAO_SUCCESS;
}

/**
 * @brief Post the logsemaphore
 * 
 * @param image 
 * @return int_fast8_t 
 */
int_fast8_t daoSemLogPost(IMAGE *image)
{
    daoTrace("\n");

#ifdef _WIN32
    ReleaseSemaphore(image->semlog, 1, NULL);
#elif defined(__APPLE__)
    unsigned int val = atomic_load(&image->md[0].semLogCounter);
    if (val < SEMAPHORE_MAXVAL)
    {
        if (sem_post(image->semlog) == 0)
        {
            atomic_fetch_add(&image->md[0].semLogCounter, 1);
        }
        else
        {
            daoError("sem_post failed on semlog: %s\n", strerror(errno));
        }
    }
    else
    {
        daoDebug("semlogCounter at max value (%d), skipping post\n", SEMAPHORE_MAXVAL);
    }
#else
    int semval = 0;
    sem_getvalue(image->semlog, &semval);
    if(semval < SEMAPHORE_MAXVAL )
    {
        sem_post(image->semlog);
    }
    #endif
    return DAO_SUCCESS;
}