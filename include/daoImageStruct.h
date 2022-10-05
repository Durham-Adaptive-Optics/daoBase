/**
 * @file    daoImageStruct.h
 * @brief   Image structure definition 
 * 
 * The IMAGE structure is defined here
 * Supports shared memory, low latency IPC through semaphores
 * 
 * Dynamic allocation within IMAGE:
 * IMAGE includes a pointer to an array of IMAGE_METADATA (usually only one element, >1 element for polymorphism)
 * IMAGE includes a pointer to an array of KEYWORDS
 * IMAGE includes a pointer to a data array
 * 
 * Inspire by CACAO Milk by O. Guyon
 * 
 * @author  S. Cetre
 * @date    23 Jun 2022
 *
 * @bug No known bugs. 
 * 
 */

#ifndef _DAOIMAGESTRUCT_H
#define _DAOIMAGESTRUCT_H

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>


#ifdef __cplusplus
extern "C"
{
#endif

// comment this line if data should not be packed
// packing data should be use with extreme care, so it is recommended to disable this feature
//#define DATA_PACKED	

#define SHAREDMEMDIR        "/tmp"        /**< location of file mapped semaphores */

#define SEMAPHORE_MAXVAL    10 	          /**< maximum value for each of the semaphore, mitigates warm-up time when processes catch up with data that has accumulated */
#define IMAGE_NB_SEMAPHORE  10            /**< Number of semaphores per image */

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

#define _DATATYPE_EVENT_UI8_UI8_UI16_UI8              20
#define SIZEOF_DATATYPE_EVENT_UI8_UI8_UI16_UI8         5

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

/** @brief Photon detection events
 * 
 * Log individual photon events on a 2D camera \n
 * Timing resolution = 1 us \n 
 * Optimized for small size \n
 * 
 * Max detector size 256 x 256 pix \n 
 * Max "exposure" time is 2^16 us = 65.535 ms (15.28 Hz) \n 
 * Wavelength resolution set by keywords in frame: \n 
 *   LAMBDA_MIN, LAMBDA_MAX \n 
 * lambda = LAMBDA_MIN + (LAMBDA_MAX-LAMBDA_MIN)/256*lambda_index \n
 * 
 * USAGE:
 * An array of EVENT_UI8_UI8_UI16_UI8 is stored in the IMAGE structure \n 
 * The array can be 1D (list of events), or 3D (N x 1 x M) for a circular buffer where the z-index (slice) is incremented between each "exposure" \n
 * md[0].cnt2 contains the number of events in the last slice written \n
 * Detection events do not have to be ordered \n
 * 
 * Write sequence in circular buffer :
 * - [1] create IMAGE structure type EVENT_UI8_UI8_UI16_UI8. Size n x 1 x m, where n = max # of event per "exposure", m = number of slices in circular buffer. Note that md[0].size[0]=m, md[0].size[1]=1, md[0].size[2]=m
 * - [2] set md[0].write=1 (start image write)
 * - [3] set k=md[0].cnt1=0 (slice index)
 * - [4] set md[0].cnt2=0 (# of events), ii=0 (event index in current slice) 
 * - [5] store time in local variable (exposure start)
 * - [6] Write each event in array.EVENT_UI8_UI8_UI16_UI8[k*md[0].size[0]+ii]. After each event, increment ii (event index)
 * - [7] When "exposure" completed, set md[0].atime to exposure time start (see step [5]), md[0].cnt1=k (last slice written),  md[0].cnt2=ii (number of events), set md[0].write=0 (write completed), increment md[0].cnt0, and post all semaphores
 * - [8] Increment k (if k=md[0].size[2], set k=0), return to step [4]
 * 
 * @warning Array size will define the maximum number of events packed in IMAGE. User is responsible for pushing out IMAGE and starting a new IMAGE or slice when max number of events is reached.
 * 
 */
typedef struct
{
	uint8_t xpix;

	uint8_t ypix;
	
	/** @brief Detection time since beginning of "exposure" [us] 
	 *
	 * Beginning of exposure is written to md[0].atime
	 *  */
	uint16_t dtus;               
	
	uint8_t lambda_index;  
#ifdef DATA_PACKED
} __attribute__ ((__packed__)) EVENT_UI8_UI8_UI16_UI8;
#else
} EVENT_UI8_UI8_UI16_UI8;
#endif

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
    
    int32_t shmfd;		     	        /**< if shared memory, file descriptor */
	// mem offset = 85

    uint64_t memsize; 			        /**< total size in memory if shared    */
	// mem offset = 93

    sem_t *semlog; 				        /**< pointer to semaphore for logging  (8 bytes on 64-bit system) */
	// mem offset = 101

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

		EVENT_UI8_UI8_UI16_UI8 *event1121;
    } array;                 	/**< pointer to data array */
	// mem offset 120

    sem_t **semptr;	                    /**< array of pointers to semaphores   (each 8 bytes on 64-bit system) */
	// mem offset 128

    IMAGE_KEYWORD *kw;
    // mem offset 136    
    
    // PID of process that read shared memory stream
    // Initialized at 0. Otherwise, when process is waiting on semaphore, its PID is written in this array
    // The array can be used to look for available semaphores
    pid_t *semReadPID;
    
    // PID of the process writing the data
    pid_t *semWritePID;
    
    
    // total size is 152 byte = 1216 bit
#ifdef DATA_PACKED
} __attribute__ ((__packed__)) IMAGE;
#else
} IMAGE;
#endif

#ifdef __cplusplus
} //extern "C"
#endif

#endif
