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
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <time.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#endif

#ifdef __APPLE__
#include <stdatomic.h>
#endif

#ifdef _WIN32
#define DLL_EXPORT __declspec(dllexport)
#else
#define DLL_EXPORT
#endif

// BOTH old and new log system are available and maintained
// New Log System
#define LOG_LEVEL_ERROR 0
#define LOG_LEVEL_WARNING 1
#define LOG_LEVEL_INFO 2
#define LOG_LEVEL_DEBUG 3
#define LOG_LEVEL_TRACE 4

#define DEFAULT_LOG_LEVEL LOG_LEVEL_INFO

#ifdef __cplusplus
extern "C" {
#endif

void daoLog(int log_level, const char *format, ...);
void daoLogError(const char *format, ...);
void daoLogPrint(const char *format, ...);
void daoLogWarning(const char *format, ...);
void daoLogInfo(const char *format, ...);
void daoLogDebug(const char *format, ...);
void daoLogTrace(const char *format, ...);
void daoLogSetLevel(int log_level);

#ifdef __cplusplus
}
#endif

// original Log System
#define DAO_SUCCESS     0
#define DAO_ERROR       1
#define DAO_TIMEOUT     -1

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
#ifdef _WIN32
// Need to find an equivalent way to get the filename with MSVC
#define __FILENAME__ __FILE__
#else
#define __FILENAME__ (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

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


#ifdef __cplusplus
extern "C"
{
#endif

// comment this line if data should not be packed
// packing data should be use with extreme care, so it is recommended to disable this feature
//#define DATA_PACKED	

#define SHAREDMEMDIR        ""        /**< location of file mapped semaphores */

#define SEMAPHORE_MAXVAL    1 	          /**< maximum value for each of the semaphore, mitigates warm-up time when processes catch up with data that has accumulated */
#define IMAGE_NB_SEMAPHORE  10            /**< Number of semaphores per image */
#define CIRCULAR_BUFFER_SIZE 1000         /**< Number of data in the cuircular buffer */

// Data types are defined as machine-independent types for portability

#define _DATATYPE_UINT8                                1  /**< uint8_t       = char */
#define SIZEOF_DATATYPE_UINT8	                       1

#define _DATATYPE_INT8                                 2  /**< int8_t   */
#define SIZEOF_DATATYPE_INT8	                       1

#define _DATATYPE_UINT16                               3  /**< uint16_t      usually = unsigned short int */
#define SIZEOF_DATATYPE_UINT16	                       2

#define _DATATYPE_INT16                                4  /**< int16_t       usually = short int          */
#define SIZEOF_DATATYPE_INT16	                       2

#define _DATATYPE_UINT32                               5  /**< uint32_t      usually = unsigned int       */
#define SIZEOF_DATATYPE_UINT32	                       4

#define _DATATYPE_INT32                                6  /**< int32_t       usually = int                */
#define SIZEOF_DATATYPE_INT32	                       4

#define _DATATYPE_UINT64                               7  /**< uint64_t      usually = unsigned long      */
#define SIZEOF_DATATYPE_UINT64	                       8

#define _DATATYPE_INT64                                8  /**< int64_t       usually = long               */
#define SIZEOF_DATATYPE_INT64	                       8

#define _DATATYPE_FLOAT                                9  /**< IEEE 754 single-precision binary floating-point format: binary32 */
#define SIZEOF_DATATYPE_FLOAT	                       4

#define _DATATYPE_DOUBLE                              10 /**< IEEE 754 double-precision binary floating-point format: binary64 */
#define SIZEOF_DATATYPE_DOUBLE	                       8

#define _DATATYPE_COMPLEX_FLOAT                       11  /**< complex_float  */
#define SIZEOF_DATATYPE_COMPLEX_FLOAT	               8

#define _DATATYPE_COMPLEX_DOUBLE                      12  /**< complex double */
#define SIZEOF_DATATYPE_COMPLEX_DOUBLE	              16

#define Dtype                                          9   /**< default data type for floating point */
#define CDtype                                        11   /**< default data type for complex */

/** @brief  Keyword
 * The IMAGE_KEYWORD structure includes :
 * 	- name
 * 	- type
 * 	- value
 */
typedef struct
{
    char name[16];         /**< keyword name                                                   */
    char type;             /**< N: unused, L: long, D: double, S: 16-char string               */

    union {
        int64_t numl;
        double  numf;
        char    valstr[16];
    } value;

    char comment[80];
#ifdef DATA_PACKED
} __attribute__ ((__packed__)) IMAGE_KEYWORD;
#else
} IMAGE_KEYWORD;
#endif

/** @brief structure holding two 8-byte integers
 * 
 * Used in an union with struct timespec to ensure fixed 16 byte length
 */
typedef struct
{
	int64_t firstlong;
	int64_t secondlong;
} TIMESPECFIXED;

typedef struct
{
    float re;
    float im;
} complex_float;

typedef struct
{
    double re;
    double im;
} complex_double;

/** @brief Image metadata
 * 
 * This structure has a fixed size regardless of implementation when packed
 * @note size = 171 byte = 1368 bit when packed
 * 
 * 
 *  
 */ 
typedef struct
{
    /** @brief Image Name */
    char name[80];   
    
    // mem offset = 80 when packed

	/** @brief Number of axis
	 * 
	 * @warning 1, 2 or 3. Values above 3 not allowed.   
	 */
    uint8_t naxis;                
    
    // mem offset = 81 when packed
    
    /** @brief Image size along each axis 
     * 
     *  If naxis = 1 (1D image), size[1] and size[2] are irrelevant
     */
    uint32_t size[3];
	
	// mem offset = 93 when packed

	/** @brief Number of elements in image
	 * 
	 * This is computed upon image creation 
	 */ 
    uint64_t nelement;             
    
    // mem offset = 101 when packed
    
    /** @brief Data type
     * 
     * Encoded according to data type defines.
     *  -  1: uint8_t
     * 	-  2: int8_t
     * 	-  3: uint16_t
     * 	-  4: int16_t
     * 	-  5: uint32_t
     * 	-  6: int32_t
     * 	-  7: uint64_t
     * 	-  8: int64_t
     * 	-  9: IEEE 754 single-precision binary floating-point format: binary32
     *  - 10: IEEE 754 double-precision binary floating-point format: binary64
     *  - 11: complex_float
     *  - 12: complex double
     * 
     */
    uint8_t atype;                 
	
	// mem offset = 102 when packed

    double creation_time;           /**< creation time (since process start)                                          */
    double last_access;             /**< last time the image was accessed  (since process start)                      */
    
    // mem offset = 118 when packed
    
    /** @brief Acquisition time (beginning of exposure   
     * 
     * atime is defined as a union to ensure fixed 16-byte length regardless of struct timespec implementation
     * 
     * Data Type: struct timespec
     * The struct timespec structure represents an elapsed time. It is declared in time.h and has the following members:
     *    time_t tv_sec
     * This represents the number of whole seconds of elapsed time.
     *    long int tv_nsec
     * This is the rest of the elapsed time (a fraction of a second), represented as the number of nanoseconds. It is always less than one billion.
     * 
     * On (most ?) 64-bit systems:
     * sizeof(struct timespec) = 16 :  sizeof(long int) = 8;  sizeof(time_t) = 8
     * 
     * @warning sizeof(struct timespec) is implementation-specific, and could be smaller that 16 byte. Users may need to create and manage their own timespec implementation if data needs to be portable across machines.
     */
    union
    {
		struct timespec ts;
		TIMESPECFIXED tsfixed;
	} atime;
    
    // mem offset = 134 when packed
    
    
    uint8_t shared;                 /**< 1 if in shared memory                                                        */
    uint8_t status;              	/**< 1 to log image (default); 0 : do not log: 2 : stop log (then goes back to 2) */
	
	// mem offset = 136 when packed

	uint8_t logflag;                    /**< set to 1 to start logging         */
    uint16_t sem; 				   
         /**< number of semaphores in use, specified at image creation      */
	
	// mem offset = 139 when packed

	uint64_t : 0; // align array to 8-byte boundary for speed  -> pushed mem offset to 144 when packed
    
    uint64_t cnt0;               	/**< counter (incremented if image is updated)                                    */
    uint64_t cnt1;               	/**< in 3D rolling buffer image, this is the last slice written                   */
    uint64_t cnt2;                  /**< in event mode, this is the # of events                                       */

    uint8_t  write;               	/**< 1 if image is being written                                                  */

    uint16_t NBkw;                  /**< number of keywords (max: 65536)                                              */
    
    // total size is 171 byte = 1368 bit when packed

    uint32_t lastPos;              /**< the 1st positon of the last write                                           */
    uint32_t lastNb;               /**< the number of last write                                                    */
    // total size is 179 bytes = 1432 bit when packed

    uint32_t packetNb;
    uint32_t packetTotal;
    // total size is 187 bytes = 1496 bit when packed
    uint64_t lastNbArray[512];
    // total size is 1211 bytes = 9688 bit when packed
#ifdef __APPLE__
        atomic_ushort semCounter[IMAGE_NB_SEMAPHORE];
        atomic_ushort semLogCounter;
#endif
#ifdef DATA_PACKED
} __attribute__ ((__packed__)) IMAGE_METADATA;
#else
} IMAGE_METADATA;
#endif

/** @brief IMAGE structure
 * The IMAGE structure includes :
 *   - an array of IMAGE_KEWORD structures
 *   - an array of IMAGE_METADATA structures (usually only 1 element)
 * 
 * @note size = 136 byte = 1088 bit
 * 
 */
typedef struct          		/**< structure used to store data arrays                      */
{
    char name[80]; 				/**< local name (can be different from name in shared memory) */
    // mem offset = 80
     
    /** @brief Image usage flag
     * 
     * 1 if image is used, 0 otherwise. \n
     * This flag is used when an array of IMAGE type is held in memory as a way to store multiple images. \n
     * When an image is freed, the corresponding memory (in array) is freed and this flag set to zero. \n
     * The active images can be listed by looking for IMAGE[i].used==1 entries.\n
     * 
     */
    uint8_t used;              
    // mem offset = 81
    
#ifdef _WIN32
	HANDLE shmfd;						/**< shared memory file handle */
	// mem offset = 89
#else
    int32_t shmfd;		     	        /**< if shared memory, file descriptor */
	// mem offset = 85
#endif

    uint64_t memsize; 			        /**< total size in memory if shared    */
	// mem offset = 93

#ifdef _WIN32
    HANDLE semlog;						/**< pointer to semaphore for logging  (8 bytes on 64-bit system) */
	// mem offset = 101
#else
	sem_t *semlog; 				        /**< pointer to semaphore for logging  (8 bytes on 64-bit system) */
	// mem offset = 101
#endif

    IMAGE_METADATA *md;			
	// mem offset = 109

	
	uint64_t : 0; // align array to 8-byte boundary for speed
	// mem offset pushed to 112
	
	/** @brief data storage array
	 * 
	 * The array is declared as a union, so that multiple data types can be supported \n
	 * 
	 * For 2D image with pixel indices ii (x-axis) and jj (y-axis), the pixel values are stored as array.<TYPE>[ jj * md[0].size[0] + ii ] \n
	 * image md[0].size[0] is x-axis size, md[0].size[1] is y-axis size
	 * 
	 * For 3D image with pixel indices ii (x-axis), jj (y-axis) and kk (z-axis), the pixel values are stored as array.<TYPE>[ kk * md[0].size[1] * md[0].size[0] + jj * md[0].size[0] + ii ] \n
	 * image md[0].size[0] is x-axis size, md[0].size[1] is y-axis size, md[0].size[2] is z-axis size
	 * 
	 * @note Up to this point, all members of the structure have a fixed memory offset to the start point
	 */ 
    union
    {
        uint8_t *UI8;  // char
        int8_t  *SI8;   

        uint16_t *UI16; // unsigned short
        int16_t *SI16;  

        uint32_t *UI32;
        int32_t *SI32;   // int

        uint64_t *UI64;        
        int64_t *SI64; // long

        float *F;
        double *D;
        
        complex_float *CF;
        complex_double *CD;

        void * V; // add so the cpp interface and reinterpret cast using templaces to required type.
    } array;                 	/**< pointer to data array */
	// mem offset 120

#ifdef _WIN32
    HANDLE *semptr;                    /**< array of pointers to semaphores   (each 8 bytes on 64-bit system) */
	// mem offset 128
#else
    sem_t **semptr;	                    /**< array of pointers to semaphores   (each 8 bytes on 64-bit system) */
	// mem offset 128
#endif

    IMAGE_KEYWORD *kw;
    // mem offset 136    
    
    // PID of process that read shared memory stream
    // Initialized at 0. Otherwise, when process is waiting on semaphore, its PID is written in this array
    // The array can be used to look for available semaphores
#ifdef _WIN32
	DWORD *semReadPID;
#else
    pid_t *semReadPID;
#endif

    // PID of the process writing the data
#ifdef _WIN32
	DWORD *semWritePID;
#else
    pid_t *semWritePID;
#endif

#ifdef _WIN32
	HANDLE shmfm;						/**< shared memory file mapping view handle */
#endif

    // total size is 152 byte = 1216 bit
    // (on Windows,  160 byte = 1280 bit)
#ifdef DATA_PACKED
} __attribute__ ((__packed__)) IMAGE;
#else
} IMAGE;
#endif

#ifdef __cplusplus
} //extern "C"
#endif


// Function declaration
#ifdef __cplusplus
extern "C" {
#endif

DLL_EXPORT void daoSetLogLevel(int logLevel);
DLL_EXPORT int daoGetLogLevel();
DLL_EXPORT unsigned daoBaseIp2Int(const char * ip); 
						   
DLL_EXPORT int_fast8_t daoShmInit1D(const char *name, uint32_t nbVal, IMAGE **image);
DLL_EXPORT int_fast8_t daoShmShm2Img(const char *name, IMAGE *image);
DLL_EXPORT int_fast8_t daoShmImage2Shm(void *im, uint32_t nbVal, IMAGE *image);
// Add new function to get blocks of data
DLL_EXPORT int_fast8_t daoShmGetDataBlock(IMAGE *image, void *out_buffer, uint32_t nFrames, int32_t semNb, struct timespec *timeout);
DLL_EXPORT int_fast8_t daoShmImagePart2Shm(char *im, uint32_t nbVal, IMAGE *image, uint32_t position,
                             uint16_t packetId, uint16_t packetTotal, uint64_t frameNumber); 
DLL_EXPORT int_fast8_t daoShmImagePart2ShmFinalize(IMAGE *image); 
DLL_EXPORT int_fast8_t daoShmImageCreateSem(IMAGE *image, long NBsem);
DLL_EXPORT int_fast8_t daoShmImageCreate(IMAGE *image, const char *name, long naxis, uint32_t *size,
                           uint8_t atype, int shared, int NBkw);
DLL_EXPORT int_fast8_t daoShmCombineShm2Shm(IMAGE **imageCude, IMAGE *image, int nbChannel, int nbVal); 
DLL_EXPORT int_fast8_t daoShmWaitForSemaphore(IMAGE *image, int32_t semNb);
DLL_EXPORT int_fast8_t daoShmWaitForSemaphoreTimeout(IMAGE *image, int32_t semNb, struct timespec timeout);
DLL_EXPORT int_fast8_t daoShmWaitForCounter(IMAGE *image);
DLL_EXPORT int_fast8_t daoShmWaitForCounter2(IMAGE *image, uint64_t counter);
DLL_EXPORT uint64_t    daoShmGetCounter(IMAGE *image);
DLL_EXPORT int_fast8_t daoShmCloseShm(IMAGE *image);
DLL_EXPORT int_fast8_t daoShmTimestampShm(IMAGE *image);

DLL_EXPORT int_fast8_t daoSemPost(IMAGE *image, int32_t semNb);
DLL_EXPORT int_fast8_t daoSemPostAll(IMAGE *image);
DLL_EXPORT int_fast8_t daoSemLogPost(IMAGE *image);

#ifdef __cplusplus
}
#endif

#endif