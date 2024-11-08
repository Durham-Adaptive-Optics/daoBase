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

#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#include <time.h>

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
#include <omp.h>
#include <zmq.h>
#include <errno.h>

#define PATH_SEPARATOR "/"

#endif

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
        daoDebug("File %s size: %zd\n", shmName, file_stat.st_size);
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

        mapv = (char*) map;
        mapv += sizeof(IMAGE_METADATA);

        daoDebug("atype = %d\n", (int) atype);
        fflush(stdout);

        if(atype == _DATATYPE_UINT8)
        {
            daoDebug("atype = UINT8\n");
            image->array.UI8 = (uint8_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT8 * image->md[0].nelement;
        }
        if(atype == _DATATYPE_INT8)
        {
            daoDebug("atype = INT8\n");
            image->array.SI8 = (int8_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT8 * image->md[0].nelement;
        }
        if(atype == _DATATYPE_UINT16)
        {
            daoDebug("atype = UINT16\n");
            image->array.UI16 = (uint16_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT16 * image->md[0].nelement;
        }
        if(atype == _DATATYPE_INT16)
        {
            daoDebug("atype = INT16\n");
            image->array.SI16 = (int16_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT16 * image->md[0].nelement;
        }
        if(atype == _DATATYPE_UINT32)
        {
            daoDebug("atype = UINT32\n");
            image->array.UI32 = (uint32_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT32 * image->md[0].nelement;
        }
        if(atype == _DATATYPE_INT32)
        {
            daoDebug("atype = INT32\n");
            image->array.SI32 = (int32_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT32 * image->md[0].nelement;
        }
        if(atype == _DATATYPE_UINT64)
        {
            daoDebug("atype = UINT64\n");
            image->array.UI64 = (uint64_t*) mapv;
            mapv += SIZEOF_DATATYPE_UINT64 * image->md[0].nelement;
        }
        if(atype == _DATATYPE_INT64)
        {
            daoDebug("atype = INT64\n");
            image->array.SI64 = (int64_t*) mapv;
            mapv += SIZEOF_DATATYPE_INT64 * image->md[0].nelement;
        }
        if(atype == _DATATYPE_FLOAT)
        {
            daoDebug("atype = FLOAT\n");
            image->array.F = (float*) mapv;
            mapv += SIZEOF_DATATYPE_FLOAT * image->md[0].nelement;
        }
        if(atype == _DATATYPE_DOUBLE)
        {
            daoDebug("atype = DOUBLE\n");
            image->array.D = (double*) mapv;
            mapv += SIZEOF_DATATYPE_COMPLEX_DOUBLE * image->md[0].nelement;
        }
        if(atype == _DATATYPE_COMPLEX_FLOAT)
        {
            daoDebug("atype = COMPLEX_FLOAT\n");
            image->array.CF = (complex_float*) mapv;
            mapv += SIZEOF_DATATYPE_COMPLEX_FLOAT * image->md[0].nelement;
        }
        if(atype == _DATATYPE_COMPLEX_DOUBLE)
        {
            daoDebug("atype = COMPLEX_DOUBLE\n");
            image->array.CD = (complex_double*) mapv;
            mapv += SIZEOF_DATATYPE_COMPLEX_DOUBLE * image->md[0].nelement;
        }
        daoDebug("%ld keywords\n", (long) image->md[0].NBkw);
        fflush(stdout);

        image->kw = (IMAGE_KEYWORD*) (mapv);
        for(kw=0; kw<image->md[0].NBkw; kw++)
        {
            if(image->kw[kw].type == 'L')
				#ifdef _WIN32
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
    int semval = 0;
    int ss;
    image->md[0].write = 1;

    if (image->md[0].atype == _DATATYPE_UINT8)
        memcpy(image->array.UI8, (unsigned char *)im, nbVal*sizeof(unsigned char)); 
    else if (image->md[0].atype == _DATATYPE_INT8)
        memcpy(image->array.SI8, (char *)im, nbVal*sizeof(char));       
    else if (image->md[0].atype == _DATATYPE_UINT16)
        memcpy(image->array.UI16, (unsigned short *)im, nbVal*sizeof(unsigned short));
    else if (image->md[0].atype == _DATATYPE_INT16)
        memcpy(image->array.SI16, (short *)im, nbVal*sizeof(short));
    else if (image->md[0].atype == _DATATYPE_INT32)
        memcpy(image->array.UI32, (unsigned int *)im, nbVal*sizeof(unsigned int));
    else if (image->md[0].atype == _DATATYPE_UINT32)
        memcpy(image->array.SI32, (int *)im, nbVal*sizeof(int));
    else if (image->md[0].atype == _DATATYPE_UINT64)
        memcpy(image->array.UI64, (unsigned long *)im, nbVal*sizeof(unsigned long));
    else if (image->md[0].atype == _DATATYPE_INT64)
        memcpy(image->array.SI64, (long *)im, nbVal*sizeof(long));
    else if (image->md[0].atype == _DATATYPE_FLOAT)
        memcpy(image->array.F, (float *)im, nbVal*sizeof(float));
    else if (image->md[0].atype == _DATATYPE_DOUBLE)
        memcpy(image->array.D, (double *)im, nbVal*sizeof(double));
    else if (image->md[0].atype == _DATATYPE_COMPLEX_FLOAT)
        memcpy(image->array.CF, (complex_float *)im, nbVal*sizeof(complex_float));
    else if (image->md[0].atype == _DATATYPE_COMPLEX_DOUBLE)
        memcpy(image->array.CD, (complex_double *)im, nbVal*sizeof(complex_double));

    for(ss = 0; ss < image->md[0].sem; ss++)
    {
		#ifdef _WIN32
		ReleaseSemaphore(image->semptr[ss], 1, NULL);
		#else
        sem_getvalue(image->semptr[ss], &semval);
        if(semval < SEMAPHORE_MAXVAL )
            sem_post(image->semptr[ss]);
		#endif
    }

    if(image->semlog != NULL)
    {
		#ifdef _WIN32
		ReleaseSemaphore(image->semlog, 1, NULL);
		#else
        sem_getvalue(image->semlog, &semval);
        if(semval < SEMAPHORE_MAXVAL)
        {
            sem_post(image->semlog);
        }
		#endif
    }

    image->md[0].write = 0;
    image->md[0].cnt0++;
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
	image->md[0].atime.tsfixed.secondlong = (1e9 * (int64_t)t.tv_sec + t.tv_nsec);

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

    if (image->md[0].atype == _DATATYPE_UINT8)
        memcpy(&image->array.UI8[position], (unsigned char *)im, nbVal*sizeof(unsigned char)); 
    else if (image->md[0].atype == _DATATYPE_INT8)
        memcpy(&image->array.SI8[position], (char *)im, nbVal*sizeof(char));       
    else if (image->md[0].atype == _DATATYPE_UINT16)
        memcpy(&image->array.UI16[position], (unsigned short *)im, nbVal*sizeof(unsigned short));
    else if (image->md[0].atype == _DATATYPE_INT16)
        memcpy(&image->array.SI16[position], (short *)im, nbVal*sizeof(short));
    else if (image->md[0].atype == _DATATYPE_INT32)
        memcpy(&image->array.UI32[position], (unsigned int *)im, nbVal*sizeof(unsigned int));
    else if (image->md[0].atype == _DATATYPE_UINT32)
        memcpy(&image->array.SI32[position], (int *)im, nbVal*sizeof(int));
    else if (image->md[0].atype == _DATATYPE_UINT64)
        memcpy(&image->array.UI64[position], (unsigned long *)im, nbVal*sizeof(unsigned long));
    else if (image->md[0].atype == _DATATYPE_INT64)
        memcpy(&image->array.SI64[position], (long *)im, nbVal*sizeof(long));
    else if (image->md[0].atype == _DATATYPE_FLOAT)
        memcpy(&image->array.F[position], (float *)im, nbVal*sizeof(float));
    else if (image->md[0].atype == _DATATYPE_DOUBLE)
        memcpy(&image->array.D[position], (double *)im, nbVal*sizeof(double));
    else if (image->md[0].atype == _DATATYPE_COMPLEX_FLOAT)
        memcpy(&image->array.CF[position], (complex_float *)im, nbVal*sizeof(complex_float));
    else if (image->md[0].atype == _DATATYPE_COMPLEX_DOUBLE)
        memcpy(&image->array.CD[position], (complex_double *)im, nbVal*sizeof(complex_double));

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
		#ifdef _WIN32
		ReleaseSemaphore(image->semptr[ss], 1, NULL);
		#else
        sem_getvalue(image->semptr[ss], &semval);
        if(semval < SEMAPHORE_MAXVAL )
            sem_post(image->semptr[ss]);
		#endif
    }

    if(image->semlog != NULL)
    {
		#ifdef _WIN32
		ReleaseSemaphore(image->semlog, 1, NULL);
		#else
        sem_getvalue(image->semlog, &semval);
        if(semval < SEMAPHORE_MAXVAL)
        {
            sem_post(image->semlog);
        }
		#endif
    }
	
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
	image->md[0].atime.tsfixed.secondlong = (1e9 * (int64_t)t.tv_sec + t.tv_nsec);
    return DAO_SUCCESS;
}

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
            if ((image->semptr[s] = sem_open(shmSemName, 0, 0644, 0))== SEM_FAILED) {
                if ((image->semptr[s] = sem_open(shmSemName, O_CREAT, 0644, 1)) == SEM_FAILED) {
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
int_fast8_t daoShmImageCreate(IMAGE *image, const char *name, long naxis, 
                              uint32_t *size, uint8_t atype, int shared, int NBkw)
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

        sharedsize = sizeof(IMAGE_METADATA);

        if(atype == _DATATYPE_UINT8)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_UINT8;
        }
        if(atype == _DATATYPE_INT8)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_INT8;
        }
        if(atype == _DATATYPE_UINT16)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_UINT16;
        }
        if(atype == _DATATYPE_INT16)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_INT16;
        }
        if(atype == _DATATYPE_INT32)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_INT32;
        }
        if(atype == _DATATYPE_UINT32)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_UINT32;
        }
        if(atype == _DATATYPE_INT64)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_INT64;
        }
        if(atype == _DATATYPE_UINT64)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_UINT64;
        }
        if(atype == _DATATYPE_FLOAT)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_FLOAT;
        }
        if(atype == _DATATYPE_DOUBLE)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_DOUBLE;
        }
        if(atype == _DATATYPE_COMPLEX_FLOAT)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_COMPLEX_FLOAT;
        }
        if(atype == _DATATYPE_COMPLEX_DOUBLE)
        {
            sharedsize += nelement*SIZEOF_DATATYPE_COMPLEX_DOUBLE;
        }

        sharedsize += NBkw*sizeof(IMAGE_KEYWORD);

		#ifdef _WIN32
        sprintf(shmName, "%s", name);
		MultiByteToWideChar(CP_UTF8, 0, shmName, -1, wideShmName, 256);
		
		PACL daclFile;
		SECURITY_ATTRIBUTES *saFile = daoCreateWindowsSecurityAttrs(0600, &daclFile);
		if (!saFile) {
			daoError("Error creating security attributes for file\n");
			exit(0);
		}

		// Create file with size required
		shmFd = CreateFileW(wideShmName, GENERIC_READ | GENERIC_WRITE,
							FILE_SHARE_READ | FILE_SHARE_WRITE,
							saFile, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL);

		if (shmFd == INVALID_HANDLE_VALUE)
		{
			DWORD error_num = GetLastError();
			daoError("Error opening file (%s) for writing, error %d\n", shmName, error_num);
			exit(0);
		}
		
		// Create file mapping handle
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
        image->md[0].shared = 1;
        image->md[0].sem = 0;
        free(nameCopy);
		
		#ifdef _WIN32
		daoDestroyWindowsSecurityAttrs(saFile, daclFile);
		#endif
    }
    else
    {
        image->md = (IMAGE_METADATA*) malloc(sizeof(IMAGE_METADATA));
        image->md[0].shared = 0;
        if(NBkw>0)
        {
            image->kw = (IMAGE_KEYWORD*) malloc(sizeof(IMAGE_KEYWORD)*NBkw);
        }
        else
        {
            image->kw = NULL;
        }
    }

    image->md[0].atype = atype;
    image->md[0].naxis = (uint8_t)naxis;

    strcpy(image->name, name); // local name
    strcpy(image->md[0].name, name);
    for(i=0; i<naxis; i++)
    {
        image->md[0].size[i] = size[i];
    }
    image->md[0].NBkw = NBkw;


    if(atype == _DATATYPE_UINT8)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA);
            image->array.UI8 = (uint8_t*) (mapv);
            memset(image->array.UI8, '\0', nelement*sizeof(uint8_t));
            mapv += sizeof(uint8_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
		}
        else
        {
            image->array.UI8 = (uint8_t*) calloc ((size_t) nelement, sizeof(uint8_t));
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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(uint8_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }

    if(atype == _DATATYPE_INT8)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA);
            image->array.SI8 = (int8_t*) (mapv);
            memset(image->array.SI8, '\0', nelement*sizeof(int8_t));
            mapv += sizeof(int8_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
		}
        else
            image->array.SI8 = (int8_t*) calloc ((size_t) nelement, sizeof(int8_t));


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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(int8_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }



    if(atype == _DATATYPE_UINT16)
    {
        if(shared==1)
        {
            mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA);
            image->array.UI16 = (uint16_t*) (mapv);
            memset(image->array.UI16, '\0', nelement*sizeof(uint16_t));
            mapv += sizeof(uint16_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.UI16 = (uint16_t*) calloc ((size_t) nelement, sizeof(uint16_t));
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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement, 1.0/1024/1024*nelement*sizeof(uint16_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }

    if(atype == _DATATYPE_INT16)
    {
        if(shared==1)
        {
            mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA);
            image->array.SI16 = (int16_t*) (mapv);
            memset(image->array.SI16, '\0', nelement*sizeof(int16_t));
            mapv += sizeof(int16_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.SI16 = (int16_t*) calloc ((size_t) nelement, sizeof(int16_t));
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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement, 1.0/1024/1024*nelement*sizeof(int16_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }


    if(atype == _DATATYPE_UINT32)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA);
            image->array.UI32 = (uint32_t*) (mapv);
            memset(image->array.UI32, '\0', nelement*sizeof(uint32_t));
            mapv += sizeof(uint32_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.UI32 = (uint32_t*) calloc ((size_t) nelement, sizeof(uint32_t));
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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(uint32_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    


    if(atype == _DATATYPE_INT32)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA);
            image->array.SI32 = (int32_t*) (mapv);
            memset(image->array.SI32, '\0', nelement*sizeof(int32_t));
            mapv += sizeof(int32_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.SI32 = (int32_t*) calloc ((size_t) nelement, sizeof(int32_t));
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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(int32_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    
    
    
    if(atype == _DATATYPE_UINT64)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA);
            image->array.UI64 = (uint64_t*) (mapv);
            memset(image->array.UI64, '\0', nelement*sizeof(uint64_t));
            mapv += sizeof(uint64_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.UI64 = (uint64_t*) calloc ((size_t) nelement, sizeof(uint64_t));
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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(uint64_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    
    if(atype == _DATATYPE_INT64)
    {
        if(shared==1)
        {
			mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA);
            image->array.SI64 = (int64_t*) (mapv);
            memset(image->array.SI64, '\0', nelement*sizeof(int64_t));
            mapv += sizeof(int64_t)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.SI64 = (int64_t*) calloc ((size_t) nelement, sizeof(int64_t));
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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(int64_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }


    if(atype == _DATATYPE_FLOAT)	
    {
        if(shared==1)
        {
            mapv = (char*) map;
            mapv += sizeof(IMAGE_METADATA);
            image->array.F = (float*) (mapv);
            memset(image->array.F, '\0', nelement*sizeof(float));
            mapv += sizeof(float)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.F = (float*) calloc ((size_t) nelement, sizeof(float));
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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(float));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    
    if(atype == _DATATYPE_DOUBLE)
    {
        if(shared==1)
        {
			mapv = (char*) map;
			mapv += sizeof(IMAGE_METADATA);
			image->array.D = (double*) (mapv);
            memset(image->array.D, '\0', nelement*sizeof(double));
            mapv += sizeof(double)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.D = (double*) calloc ((size_t) nelement, sizeof(double));
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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(double));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    
    if(atype == _DATATYPE_COMPLEX_FLOAT)
    {
        if(shared==1)
        {
			mapv = (char*) map;
			mapv += sizeof(IMAGE_METADATA);
			image->array.CF = (complex_float*) (mapv);			
            memset(image->array.CF, '\0', nelement*sizeof(complex_float));
            mapv += sizeof(complex_float)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.CF = (complex_float*) calloc ((size_t) nelement, sizeof(complex_float));
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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(complex_float));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }
    
    if(atype == _DATATYPE_COMPLEX_DOUBLE)
    {
        if(shared==1)
        {
			mapv = (char*) map;
			mapv += sizeof(IMAGE_METADATA);
			image->array.CD = (complex_double*) (mapv);			
            memset(image->array.CD, '\0', nelement*sizeof(complex_double));
            mapv += sizeof(complex_double)*nelement;
            image->kw = (IMAGE_KEYWORD*) (mapv);
        }
        else
        {
            image->array.CD = (complex_double*) calloc ((size_t) nelement,sizeof(complex_double));
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
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(complex_double));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }

    clock_gettime(CLOCK_REALTIME, &timenow);
    image->md[0].last_access = 1.0*timenow.tv_sec + 0.000000001*timenow.tv_nsec;
    image->md[0].creation_time = image->md[0].last_access;
    image->md[0].write = 0;
    image->md[0].cnt0 = 0;
    image->md[0].cnt1 = 0;
    image->md[0].nelement = nelement;

    if(shared==1)
    {
        daoInfo("Creating Semaphores\n");
        daoImageCreateSem(image, 10); // by default, create 10 semaphores
        daoInfo("Semaphores created\n");
    }
    else
    {
		image->md[0].sem = 0; // no semaphores
    }

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
    int semval = 0;
    int ss;
    int pp;
    int k;
    image->md[0].write = 1;
    
    // check type and use proper array
    if (image->md[0].atype == _DATATYPE_UINT8)
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
    else if (image->md[0].atype == _DATATYPE_INT8)
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
    else if (image->md[0].atype == _DATATYPE_UINT16)
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
    else if (image->md[0].atype == _DATATYPE_INT16)
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
    else if (image->md[0].atype == _DATATYPE_UINT32)
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
    else if (image->md[0].atype == _DATATYPE_INT32)
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
    else if (image->md[0].atype == _DATATYPE_UINT64)
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
    else if (image->md[0].atype == _DATATYPE_INT64)
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
    else if (image->md[0].atype == _DATATYPE_FLOAT)
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
    else if (image->md[0].atype == _DATATYPE_DOUBLE)
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
    else if (image->md[0].atype == _DATATYPE_COMPLEX_FLOAT)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.CF[pp].re = 0;
            image[0].array.CF[pp].im = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.CF[pp].re += imageCube[k][0].array.CF[pp].re;
                image[0].array.CF[pp].im += imageCube[k][0].array.CF[pp].im;
            }
        }
    }
        else if (image->md[0].atype == _DATATYPE_COMPLEX_DOUBLE)
    {
        for (pp=0; pp<nbVal; pp++)
        {   
            image[0].array.CD[pp].re = 0;
            image[0].array.CD[pp].im = 0;
            for(k=0;k<nbChannel;k++) 
            {
                image[0].array.CD[pp].re += imageCube[k][0].array.CD[pp].re;
                image[0].array.CD[pp].im += imageCube[k][0].array.CD[pp].im;
            }
        }
    }
	
    for(ss = 0; ss < image->md[0].sem; ss++)
    {
		#ifdef _WIN32
		ReleaseSemaphore(image->semptr[ss], 1, NULL);
		#else
        sem_getvalue(image->semptr[ss], &semval);
        if(semval < SEMAPHORE_MAXVAL )
            sem_post(image->semptr[ss]);
		#endif
    }

    if(image->semlog != NULL)
    {
		#ifdef _WIN32
		ReleaseSemaphore(image->semlog, 1, NULL);
		#else
        sem_getvalue(image->semlog, &semval);
        if(semval < SEMAPHORE_MAXVAL)
        {
            sem_post(image->semlog);
        }
		#endif
    }

    image->md[0].write = 0;
    image->md[0].cnt0++;
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
	image->md[0].atime.tsfixed.secondlong = (1e9 * (int64_t)t.tv_sec + t.tv_nsec);

    return DAO_SUCCESS;
}

/**
 * @brief Wait for new data in SHM
 * 
 * @param image 
 * @param semNb semaphore number
 * @param timeout in seconds
 * @return uint_fast64_t 
 * 
 */
int_fast8_t daoShmWaitForSemaphore(IMAGE *image, int32_t semNb, int32_t timeout)
{
    daoTrace("\n");
    // Wait for new image
	#ifdef _WIN32
	DWORD dWaitResult;
    if (timeout == 0) {
		// Wait indefinitely
		dWaitResult = WaitForSingleObject(image->semptr[semNb], INFINITE);
	} else {
		// Convert seconds to milliseconds for timeout
		dWaitResult = WaitForSingleObject(image->semptr[semNb], timeout * 1000);
	}
	switch (dWaitResult) {
		case WAIT_OBJECT_0:
			break;
        case WAIT_TIMEOUT:
			return DAO_ERROR;
		default:
			daoError("Unexpected result from WaitForSingleObject (Windows system error no. %d).\n", GetLastError());
			return DAO_ERROR;
	}
	#else
    if (timeout == 0)
    {
        // Wait for new image indefinitively
        sem_wait(image->semptr[semNb]);
        return DAO_SUCCESS;
    }
    else
    {
        struct timespec tOut;
        clock_gettime(CLOCK_REALTIME, &tOut);
        tOut.tv_sec += timeout; // second timeout
        // Wait for new image
        //sem_wait(image->semptr[semNb]);
        if (sem_timedwait(image->semptr[semNb], &tOut) != -1)
        {
            return DAO_SUCCESS;
        }
        else
        {
            return DAO_ERROR;
        }
    }
	#endif
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
	#ifdef _WIN32
	volatile uint_fast64_t counter = image->md[0].cnt0;
	while (image->md[0].cnt0 <= counter)
	{
		// Spin
	}
	#else
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
//    if (image->md[0].atype == _DATATYPE_UINT8)
//    {
//                image[0].array.UI8[pp] += imageCube[k][0].array.UI8[pp];
//    }
//    else if (image->md[0].atype == _DATATYPE_INT8)
//    {
//                image[0].array.SI8[pp] += imageCube[k][0].array.SI8[pp];
//    }
//    else if (image->md[0].atype == _DATATYPE_UINT16)
//    {
//                image[0].array.UI16[pp] += imageCube[k][0].array.UI16[pp];
//    }
//    else if (image->md[0].atype == _DATATYPE_INT16)
//    {
//                image[0].array.SI16[pp] += imageCube[k][0].array.SI16[pp];
//    }
//    else if (image->md[0].atype == _DATATYPE_UINT32)
//    {
//                image[0].array.UI32[pp] += imageCube[k][0].array.UI32[pp];
//    }
//    else if (image->md[0].atype == _DATATYPE_INT32)
//    {
//                image[0].array.SI32[pp] += imageCube[k][0].array.SI32[pp];
//    }
//    else if (image->md[0].atype == _DATATYPE_UINT64)
//    {
//                image[0].array.UI64[pp] += imageCube[k][0].array.UI64[pp];
//    }
//    else if (image->md[0].atype == _DATATYPE_INT64)
//    {
//                image[0].array.SI64[pp] += imageCube[k][0].array.SI64[pp];
//    }
//    else if (image->md[0].atype == _DATATYPE_FLOAT)
//    {
//                image[0].array.F[pp] += imageCube[k][0].array.F[pp];
//    }
//    else if (image->md[0].atype == _DATATYPE_DOUBLE)
//    {
//                image[0].array.D[pp] += imageCube[k][0].array.D[pp];
//    }
//    return DAO_SUCCESS;
//}

// Function to calculate array size based on data type and number of elements
size_t calculateArraySize(uint8_t atype, uint64_t nelement) 
{
    switch(atype) 
    {
        case _DATATYPE_UINT8:
            return nelement * sizeof(uint8_t);
        case _DATATYPE_INT8:
            return nelement * sizeof(int8_t);
        case _DATATYPE_UINT16:
            return nelement * sizeof(uint16_t);
        case _DATATYPE_INT16:
            return nelement * sizeof(int16_t);
        case _DATATYPE_UINT32:
            return nelement * sizeof(uint32_t);
        case _DATATYPE_INT32:
            return nelement * sizeof(int32_t);
        case _DATATYPE_UINT64:
            return nelement * sizeof(uint64_t);
        case _DATATYPE_INT64:
            return nelement * sizeof(int64_t);
        case _DATATYPE_FLOAT:
            return nelement * sizeof(float);
        case _DATATYPE_DOUBLE:
            return nelement * sizeof(double);
        case _DATATYPE_COMPLEX_FLOAT:
            return nelement * sizeof(complex_float);
        case _DATATYPE_COMPLEX_DOUBLE:
            return nelement * sizeof(complex_double);
        default:
            return 0;  // Invalid data type
    }
}

// Serialize the IMAGE structure
int_fast8_t serializeImage(IMAGE *image, char *buffer) 
{
    daoTrace("\n");
    IMAGE_SERIALIZED serialized;
    
    // Copy fields
    strncpy(serialized.name, image->name, sizeof(serialized.name));
    serialized.used = image->used;
    serialized.shmfd = image->shmfd;
    serialized.memsize = image->memsize;
    serialized.md = *image->md;
    serialized.array_size = calculateArraySize(image->md->atype, image->md->nelement);
    
    // Serialize the main fields
    memcpy(buffer, &serialized, sizeof(IMAGE_SERIALIZED));
    buffer += sizeof(IMAGE_SERIALIZED);
    
    // Serialize array data
    size_t array_size = serialized.array_size;
    if (array_size > 0) 
    {
        switch (image->md->atype) 
        {
            case _DATATYPE_UINT8:
                memcpy(buffer, image->array.UI8, array_size);
                break;
            case _DATATYPE_INT8:
                memcpy(buffer, image->array.SI8, array_size);
                break;
            case _DATATYPE_UINT16:
                memcpy(buffer, image->array.UI16, array_size);
                break;
            case _DATATYPE_INT16:
                memcpy(buffer, image->array.SI16, array_size);
                break;
            case _DATATYPE_UINT32:
                memcpy(buffer, image->array.UI32, array_size);
                break;
            case _DATATYPE_INT32:
                memcpy(buffer, image->array.SI32, array_size);
                break;
            case _DATATYPE_UINT64:
                memcpy(buffer, image->array.UI64, array_size);
                break;
            case _DATATYPE_INT64:
                memcpy(buffer, image->array.SI64, array_size);
                break;
            case _DATATYPE_FLOAT:
                memcpy(buffer, image->array.F, array_size);
                break;
            case _DATATYPE_DOUBLE:
                memcpy(buffer, image->array.D, array_size);
                break;
            case _DATATYPE_COMPLEX_FLOAT:
                memcpy(buffer, image->array.CF, array_size);
                break;
            case _DATATYPE_COMPLEX_DOUBLE:
                memcpy(buffer, image->array.CD, array_size);
                break;
        }
    }
    return DAO_SUCCESS;
}

size_t calculateBufferSize(IMAGE *image)
{
    // Base size: serialized structure size (without semaphores, etc.)
    size_t size = sizeof(IMAGE_SERIALIZED); 
    daoDebug("buffer_size = %ld\n", size);

    // Add the size of the serialized array data
    size += calculateArraySize(image->md->atype, image->md->nelement);
    daoDebug("buffer_size = %ld\n", size);
    
    return size;
}


// Deserialize the IMAGE structure
int_fast8_t deserializeImage(char *buffer, IMAGE *image) 
{
    daoTrace("\n");
    IMAGE_SERIALIZED serialized;
    
    // Deserialize the main fields
    memcpy(&serialized, buffer, sizeof(IMAGE_SERIALIZED));
    buffer += sizeof(IMAGE_SERIALIZED);
    
    // Set the fields in the IMAGE structure
    strncpy(image->name, serialized.name, sizeof(image->name));
    image->used = serialized.used;
    image->shmfd = serialized.shmfd;
    image->memsize = serialized.memsize;
    //image->md = (IMAGE_METADATA *)malloc(sizeof(IMAGE_METADATA));
    //*image->md = serialized.md;
    memcpy(image->md, &serialized.md, sizeof(IMAGE_METADATA));
    
    // Allocate memory for the array based on the data type
    size_t array_size = serialized.array_size;
    if (array_size > 0) 
    {
        switch (image->md->atype) 
        {
            case _DATATYPE_UINT8:
//                image->array.UI8 = (uint8_t *)malloc(array_size);
                memcpy(image->array.UI8, buffer, array_size);
                break;
            case _DATATYPE_INT8:
//                image->array.SI8 = (int8_t *)malloc(array_size);
                memcpy(image->array.SI8, buffer, array_size);
                break;
            case _DATATYPE_UINT16:
//                image->array.UI16 = (uint16_t *)malloc(array_size);
                memcpy(image->array.UI16, buffer, array_size);
                break;
            case _DATATYPE_INT16:
//                image->array.SI16 = (int16_t *)malloc(array_size);
                memcpy(image->array.SI16, buffer, array_size);
                break;
            case _DATATYPE_UINT32:
//                image->array.UI32 = (uint32_t *)malloc(array_size);
                memcpy(image->array.UI32, buffer, array_size);
                break;
            case _DATATYPE_INT32:
//                image->array.SI32 = (int32_t *)malloc(array_size);
                memcpy(image->array.SI32, buffer, array_size);
                break;
            case _DATATYPE_UINT64:
//                image->array.UI64 = (uint64_t *)malloc(array_size);
                memcpy(image->array.UI64, buffer, array_size);
                break;
            case _DATATYPE_INT64:
//                image->array.SI64 = (int64_t *)malloc(array_size);
                memcpy(image->array.SI64, buffer, array_size);
                break;
            case _DATATYPE_FLOAT:
//                image->array.F = (float *)malloc(array_size);
                memcpy(image->array.F, buffer, array_size);
                break;
            case _DATATYPE_DOUBLE:
//                image->array.D = (double *)malloc(array_size);
                memcpy(image->array.D, buffer, array_size);
                break;
            case _DATATYPE_COMPLEX_FLOAT:
//                image->array.CF = (complex_float *)malloc(array_size);
                memcpy(image->array.CF, buffer, array_size);
                break;
            case _DATATYPE_COMPLEX_DOUBLE:
//                image->array.CD = (complex_double *)malloc(array_size);
                memcpy(image->array.CD, buffer, array_size);
                break;
        }
    }
    return DAO_SUCCESS;
}

// ZeroMQ send function
int_fast8_t zmqSendImageTCP(IMAGE *image, void *socket) 
{
    daoTrace("\n");
    size_t buffer_size = calculateBufferSize(image);

    // Dynamically allocate buffer
    char *buffer = (char *)malloc(buffer_size);
    if (buffer == NULL) 
    {
        daoError("Failed to allocate memory for serialization\n");
        return DAO_ERROR;
    }

    serializeImage(image, buffer);

    zmq_msg_t message;
    zmq_msg_init_size(&message, buffer_size);
    memcpy(zmq_msg_data(&message), buffer, buffer_size);
    
    // Send message over ZMQ_RADIO socket
    int rc = zmq_msg_send(&message, socket, 0);
    if (rc == -1) 
    {
        daoError("Failed to send message\n");
    }

    zmq_msg_close(&message);
    free(buffer);
    return (rc == -1) ? DAO_ERROR : DAO_SUCCESS;
}

// ZeroMQ receive function
int_fast8_t zmqReceiveImageTCP(IMAGE *image, void *socket) 
{
    daoTrace("\n");
    zmq_msg_t message;
    zmq_msg_init(&message);

    if (zmq_msg_recv(&message, socket, 0) == -1)
    {
        zmq_msg_close(&message);
        return DAO_ERROR;
    }

    char *buffer = (char *)zmq_msg_data(&message);
    deserializeImage(buffer, image);

    zmq_msg_close(&message);
    return DAO_SUCCESS;
}

// ZeroMQ send functio UDPn
int_fast8_t zmqSendImageUDP(IMAGE *image, void *socket, const char *group,
                            size_t maxPayload) 
{
    daoTrace("\n");
    
    // Send in chunks
    size_t offset = 0;
    size_t remaining;
    int frameId = image->md->cnt0;
    int isLastPacket = 0;
    size_t message_size;
    size_t chunk_size;
    size_t total_size = 0;
    int sequenceNumber = 0; // Reset at the beginning of each frame
//    double elapsed_time;
//    double total_time = 0;
    // Start time measurement
//    struct timespec sendStart, sendEnd;

    size_t buffer_size = calculateBufferSize(image);

    remaining = buffer_size;

    // Dynamically allocate buffer
    char *buffer = (char *)malloc(buffer_size);
    if (buffer == NULL) 
    {
        daoError("Failed to allocate memory for serialization\n");
        return DAO_ERROR;
    }

    serializeImage(image, buffer);

    // Initialize ZeroMQ message with total size (group name + image data)
    zmq_msg_t message;
    zmq_msg_init_size(&message, buffer_size);
    memcpy(zmq_msg_data(&message), buffer, buffer_size);

    while (remaining > 0) 
    {
        // Determine if this is the last packet before sending
        isLastPacket = remaining <= maxPayload ? 1 : 0;

        chunk_size = remaining > maxPayload ? maxPayload : remaining;

        // Calculate total message size (frameId + flag + chunk data)
        message_size = sizeof(frameId) + sizeof(isLastPacket) + sizeof(sequenceNumber) + chunk_size;
        total_size += message_size;
        
        zmq_msg_t message;
        zmq_msg_init_size(&message, message_size);
        
        // Set frameId and isLastPacket flag in the message
        memcpy(zmq_msg_data(&message), &frameId, sizeof(frameId));
        memcpy((char *)zmq_msg_data(&message) + sizeof(frameId), &isLastPacket, sizeof(isLastPacket));
        memcpy((char *)zmq_msg_data(&message) + sizeof(frameId) + sizeof(isLastPacket), &sequenceNumber, sizeof(sequenceNumber));

        // Copy the chunk data to the message
        memcpy((char *)zmq_msg_data(&message) + sizeof(frameId) + sizeof(isLastPacket) + sizeof(sequenceNumber), buffer + offset, chunk_size);

//        clock_gettime(CLOCK_MONOTONIC, &sendStart);
        // Set the group for the message
        zmq_msg_set_group(&message, group);  // Group name ≤ 13 characters
        // Send the chunk
        int rc = zmq_msg_send(&message, socket, 0);
        if (rc == -1) {
            daoError("Failed to send message: %s\n", zmq_strerror(errno));
            zmq_msg_close(&message);
            free(buffer);
            return DAO_ERROR;
        }
        sequenceNumber++; // Increment sequence number for each packet in the frame

        zmq_msg_close(&message);
        offset += chunk_size;
        remaining -= chunk_size;
//        clock_gettime(CLOCK_MONOTONIC, &sendEnd);
//        elapsed_time = (sendEnd.tv_sec - sendStart.tv_sec) * 1e6 + (sendEnd.tv_nsec - sendStart.tv_nsec) / 1e3;
//        daoInfo("Sent packet %d of size %ld for frame %d in %lf usec\n", sequenceNumber, message_size, frameId, elapsed_time);
    }

    free(buffer);

//    daoInfo("Total send = %ld in time: %.3f microseconds\n", total_size, total_time);
    return DAO_SUCCESS;
}

// ZeroMQ receive function UDP
int_fast8_t zmqReceiveImageUDP(IMAGE *image, void *socket) 
{
    daoTrace("\n");

    size_t offset = 0;  // Track the current write position within the buffer
    int expectedFrameId = -1;  // Initialize to an invalid frame ID to check on the first packet
    int expectedSequenceNumber = 0;
    int isLastPacket = 0;
    int receivedFrameId;
    int receivedSequenceNumber;
    size_t chunk_size;
    double elapsed_time;
    double total_time = 0;
    size_t total_size = 0;
    struct timespec recvStart, recvEnd;

    size_t buffer_size = calculateBufferSize(image);  // Determine total size for reassembled data

    // Allocate buffer for reassembly of the full image data
    char *buffer = (char *)malloc(buffer_size);
    if (buffer == NULL) {
        daoError("Failed to allocate memory for reassembly\n");
        return DAO_ERROR;
    }

    while (offset < buffer_size && !isLastPacket) 
    {
        clock_gettime(CLOCK_MONOTONIC, &recvStart);
        zmq_msg_t message;
        zmq_msg_init(&message);

        //zmq_msg_set_group(&message, group);  // Group name 
        // Receive a chunk of data from the ZMQ_DISH socket
        if (zmq_msg_recv(&message, socket, 0) == -1)
        {
            // Timeout or error; reset sync if timeout occurs
            //daoError("Receive timed out or failed, attempting to resynchronize\n");
            expectedFrameId = -1;
            expectedSequenceNumber = 0;
            zmq_msg_close(&message);
            free(buffer);
            return DAO_ERROR;
        }
    
        chunk_size = zmq_msg_size(&message);
        total_size += chunk_size;
        const char *data = (const char *)zmq_msg_data(&message);

        memcpy(&receivedFrameId, data, sizeof(receivedFrameId));
        memcpy(&isLastPacket, data + sizeof(receivedFrameId), sizeof(isLastPacket));
        memcpy(&receivedSequenceNumber, data + sizeof(receivedFrameId) + sizeof(isLastPacket), sizeof(receivedSequenceNumber));

        // Check if we're starting a new frame
        if (receivedSequenceNumber == 0) {
            expectedFrameId = receivedFrameId;
            expectedSequenceNumber = 0;
            offset = 0;  // Reset buffer offset for new frame
        }

        // Verify frame ID and sequence number
        if (receivedFrameId != expectedFrameId || receivedSequenceNumber != expectedSequenceNumber) {
            daoError("Frame or sequence mismatch: expected frame %d seq %d but received frame %d seq %d. Resynchronizing...\n",
                     expectedFrameId, expectedSequenceNumber, receivedFrameId, receivedSequenceNumber);
            // Reset expected frame and sequence, wait for next frame start
            expectedFrameId = -1;
            expectedSequenceNumber = 0;
            zmq_msg_close(&message);
            free(buffer);
            return DAO_ERROR;
        }

        // Calculate the start of the actual data after frameId and isLastPacket
        size_t header_size = sizeof(receivedFrameId) + sizeof(isLastPacket) + sizeof(receivedSequenceNumber);
        size_t data_size = chunk_size - header_size;

        // Ensure the chunk fits within the remaining buffer
        if (offset + data_size > buffer_size) 
        {
            daoError("Received data (%zu) exceeds expected buffer size = %zu\n", offset + data_size, buffer_size);
            zmq_msg_close(&message);
            free(buffer);
            return DAO_ERROR;
        }

        // Copy the data portion (after frameId and isLastPacket) into the buffer
        memcpy(buffer + offset, data + header_size, data_size);
        zmq_msg_close(&message);  // Close message to release resources
        
        offset += data_size;  // Update the offset to track the total received data
        expectedSequenceNumber++;

        clock_gettime(CLOCK_MONOTONIC, &recvEnd);
        elapsed_time = (recvEnd.tv_sec - recvStart.tv_sec) * 1e6 + (recvEnd.tv_nsec - recvStart.tv_nsec) / 1e3;
        // skip the first time measurment since it wil have the whole wait for the first packet
        if (receivedSequenceNumber > 1)
        {
            total_time += elapsed_time;
        }
//        daoInfo("Received packet %d of size %ld for frame %d in %lf usec\n", receivedSequenceNumber, chunk_size, receivedFrameId, elapsed_time);
    }

    // Deserialize the full image from the buffer
    int result = deserializeImage(buffer, image);

    // Free the reassembly buffer after deserialization
    free(buffer);

//    daoInfo("Total receive = %ld in time: %.3f microseconds\n", total_size, total_time);

    return result == 0 ? DAO_SUCCESS : DAO_ERROR;  // Check deserialization success
}