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
// SHM
int IMAGE_INDEX = 0;
int NBIMAGES = 10;

/**
 * Return a time stamp string with microsecond precision
 */
char * daoGetTimeStamp()
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
 * Extract image from a shared memory
 */
int_fast8_t daoShm2Img(const char *name, char *prefix, IMAGE *image)
{
    daoTrace("\n");
    int SM_fd;
    struct stat file_stat;
    char SM_fname[200];
    IMAGE_METADATA *map;
    char *mapv;
    uint8_t atype;
    int kw;
    char sname[200];
    sem_t *stest;
    int sOK;
    long snb;
    long s;

    int rval = DAO_ERROR;
    sprintf(SM_fname, "%s/%s%s.im.shm", SHAREDMEMDIR, prefix, name);

    SM_fd = open(SM_fname, O_RDWR);
    if(SM_fd==-1)
    {
        image->used = 0;
        daoError("Cannot import shared memory file %s \n", name);
        rval = DAO_ERROR;
    }
    else
    {
        rval = DAO_SUCCESS; // we assume by default success

        fstat(SM_fd, &file_stat);
        daoDebug("File %s size: %zd\n", SM_fname, file_stat.st_size);
        map = (IMAGE_METADATA*) mmap(0, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, SM_fd, 0);
        if (map == MAP_FAILED) 
        {
            close(SM_fd);
            perror("Error mmapping the file");
            rval = DAO_ERROR;
            exit(0);
        }

        image->memsize = file_stat.st_size;
        image->shmfd = SM_fd;
        image->md = map;
        //        image->md[0].sem = 0;
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
                daoDebug("%d  %s %ld %s\n", kw, image->kw[kw].name, image->kw[kw].value.numl, image->kw[kw].comment);
            if(image->kw[kw].type == 'D')
                daoDebug("%d  %s %lf %s\n", kw, image->kw[kw].name, image->kw[kw].value.numf, image->kw[kw].comment);
            if(image->kw[kw].type == 'S')
                daoDebug("%d  %s %s %s\n", kw, image->kw[kw].name, image->kw[kw].value.valstr, image->kw[kw].comment);
        }

        mapv += sizeof(IMAGE_KEYWORD)*image->md[0].NBkw;
        strcpy(image->name, name);
        // looking for semaphores
        snb = 0;
        sOK = 1;
        while(sOK==1)
        {
            sprintf(sname, "%s_sem%02ld", image->md[0].name, snb);
            daoDebug("semaphore %s\n", sname);
            if((stest = sem_open(sname, 0, 0644, 0))== SEM_FAILED)
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
            sprintf(sname, "%s_sem%02ld", image->md[0].name, s);
            if ((image->semptr[s] = sem_open(sname, 0, 0644, 0))== SEM_FAILED) 
            {
                daoError("could not open semaphore %s\n", sname);
            }
        }

        sprintf(sname, "%s_semlog", image->md[0].name);
        if ((image->semlog = sem_open(sname, 0, 0644, 0))== SEM_FAILED) 
        {
            daoWarning("could not open semaphore %s\n", sname);
        }
    }
    return(rval);
}

/**
 * Init 1D array in shared memory
 */
int_fast8_t daoInit1D(const char *name, char *prefix, int nbVal, IMAGE **image)
{
    daoTrace("\n");
    int naxis = 2;
    char fullName[64];
    sprintf(fullName, "%s%s", prefix, name);

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

    daoImageCreate(*image, fullName, naxis, imsize, _DATATYPE_FLOAT, 1, 0);

    return DAO_SUCCESS;
}

/*
 * The image is send to the shared memory.
 */
int_fast8_t daoImage2Shm(void *procim, int nbVal, IMAGE *image) 
{
    daoTrace("\n");
    int semval = 0;
    int ss;
    //int pp;
    image[IMAGE_INDEX].md[0].write = 1;

    //for(pp = 0; pp < nbVal; pp++)
    //{
    //        image[IMAGE_INDEX].array.F[pp] = procim[pp];
    //}
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_UINT8)
        memcpy(image[IMAGE_INDEX].array.UI8, (unsigned char *)procim, nbVal*sizeof(unsigned char)); 
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_INT8)
        memcpy(image[IMAGE_INDEX].array.SI8, (char *)procim, nbVal*sizeof(char));       
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_UINT16)
        memcpy(image[IMAGE_INDEX].array.UI16, (unsigned short *)procim, nbVal*sizeof(unsigned short));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_INT16)
        memcpy(image[IMAGE_INDEX].array.SI16, (short *)procim, nbVal*sizeof(short));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_INT32)
        memcpy(image[IMAGE_INDEX].array.UI32, (unsigned int *)procim, nbVal*sizeof(unsigned int));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_UINT32)
        memcpy(image[IMAGE_INDEX].array.SI32, (int *)procim, nbVal*sizeof(int));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_UINT64)
        memcpy(image[IMAGE_INDEX].array.UI64, (unsigned long *)procim, nbVal*sizeof(unsigned long));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_INT64)
        memcpy(image[IMAGE_INDEX].array.SI64, (long *)procim, nbVal*sizeof(long));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_FLOAT)
        memcpy(image[IMAGE_INDEX].array.F, (float *)procim, nbVal*sizeof(float));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_DOUBLE)
        memcpy(image[IMAGE_INDEX].array.D, (double *)procim, nbVal*sizeof(double));

    for(ss = 0; ss < image[IMAGE_INDEX].md[0].sem; ss++)
    {
        sem_getvalue(image[IMAGE_INDEX].semptr[ss], &semval);
        if(semval < SEMAPHORE_MAXVAL )
            sem_post(image[IMAGE_INDEX].semptr[ss]);
    }

    if(image[IMAGE_INDEX].semlog != NULL)
    {
        sem_getvalue(image[IMAGE_INDEX].semlog, &semval);
        if(semval < SEMAPHORE_MAXVAL)
        {
            sem_post(image[IMAGE_INDEX].semlog);
        }
    }

    image[IMAGE_INDEX].md[0].write = 0;
    image[IMAGE_INDEX].md[0].cnt0++;
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    image[IMAGE_INDEX].md[0].atime.tsfixed.secondlong = (unsigned long)(1e9 * t.tv_sec + t.tv_nsec);

    return DAO_SUCCESS;
}
/*
 * The image is send to the shared memory.
 * No release of semaphore since it is a part write
 */
int_fast8_t daoImagePart2Shm(void *procim, int nbVal, IMAGE *image, int position, unsigned short packetId, unsigned packetTotal) 
{
    daoTrace("\n");
    //int pp;
    image[IMAGE_INDEX].md[0].write = 1;

    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_UINT8)
        memcpy(&image[IMAGE_INDEX].array.UI8[position], (unsigned char *)procim, nbVal*sizeof(unsigned char)); 
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_INT8)
        memcpy(&image[IMAGE_INDEX].array.SI8[position], (char *)procim, nbVal*sizeof(char));       
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_UINT16)
        memcpy(&image[IMAGE_INDEX].array.UI16[position], (unsigned short *)procim, nbVal*sizeof(unsigned short));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_INT16)
        memcpy(&image[IMAGE_INDEX].array.SI16[position], (short *)procim, nbVal*sizeof(short));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_INT32)
        memcpy(&image[IMAGE_INDEX].array.UI32[position], (unsigned int *)procim, nbVal*sizeof(unsigned int));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_UINT32)
        memcpy(&image[IMAGE_INDEX].array.SI32[position], (int *)procim, nbVal*sizeof(int));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_UINT64)
        memcpy(&image[IMAGE_INDEX].array.UI64[position], (unsigned long *)procim, nbVal*sizeof(unsigned long));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_INT64)
        memcpy(&image[IMAGE_INDEX].array.SI64[position], (long *)procim, nbVal*sizeof(long));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_FLOAT)
        memcpy(&image[IMAGE_INDEX].array.F[position], (float *)procim, nbVal*sizeof(float));
    if (image[IMAGE_INDEX].md[0].atype == _DATATYPE_DOUBLE)
        memcpy(&image[IMAGE_INDEX].array.D[position], (double *)procim, nbVal*sizeof(double));

    image[IMAGE_INDEX].md[0].lastPos = position;
    image[IMAGE_INDEX].md[0].lastNb = nbVal;
    image[IMAGE_INDEX].md[0].packetNb = packetId;
    image[IMAGE_INDEX].md[0].packetTotal = packetTotal;
    image[IMAGE_INDEX].md[0].lastNbArray[packetId-1]++;
    image[IMAGE_INDEX].md[0].write = 0;

    return DAO_SUCCESS;
}

/*
 * The image has beed sent to the shared memory.
 * Release of semaphore since it is a part write
 */
int_fast8_t daoImagePart2ShmFinalize(IMAGE *image) 
{
    daoTrace("\n");
    int semval = 0;
    int ss;
    for(ss = 0; ss < image[IMAGE_INDEX].md[0].sem; ss++)
    {
        sem_getvalue(image[IMAGE_INDEX].semptr[ss], &semval);
        if(semval < SEMAPHORE_MAXVAL )
            sem_post(image[IMAGE_INDEX].semptr[ss]);
    }

    if(image[IMAGE_INDEX].semlog != NULL)
    {
        sem_getvalue(image[IMAGE_INDEX].semlog, &semval);
        if(semval < SEMAPHORE_MAXVAL)
        {
            sem_post(image[IMAGE_INDEX].semlog);
        }
    }
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    image[IMAGE_INDEX].md[0].atime.tsfixed.secondlong = (unsigned long)(1e9 * t.tv_sec + t.tv_nsec);
    image[IMAGE_INDEX].md[0].cnt0++;
    return DAO_SUCCESS;
}

int_fast8_t daoImageCreateSem(IMAGE *image, long NBsem)
{
    char sname[200];
    long s, s1;
//    int r;
//    char command[200];
    char fname[200];
//    int semfile[100];

	
	// Remove pre-existing semaphores if any
    if(image->md[0].sem != NBsem)
    {
        // Close existing semaphores ...
        for(s=0; s < image->md[0].sem; s++)
            sem_close(image->semptr[s]);
        image->md[0].sem = 0;

		// ... and remove associated files
        for(s1=NBsem; s1<100; s1++)
        {
            sprintf(fname, "/dev/shm/sem.%s_sem%02ld", image->md[0].name, s1);
            daoDebug("removing %s\n", fname);
            remove(fname);
        }
        daoDebug("Done\n");
        free(image->semptr);
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
            sprintf(sname, "%s_sem%02ld", image->md[0].name, s);
            if ((image->semptr[s] = sem_open(sname, 0, 0644, 0))== SEM_FAILED) {
                if ((image->semptr[s] = sem_open(sname, O_CREAT, 0644, 1)) == SEM_FAILED) {
                    perror("semaphore initilization");
                }
                else
                    sem_init(image->semptr[s], 1, 0);
            }
        }
    }
    
    return(0);
}





int_fast8_t daoImageCreate(IMAGE *image, const char *name, long naxis, uint32_t *size, uint8_t atype, int shared, int NBkw)
{
    long i;//,ii;
//    time_t lt;
    long nelement;
    struct timespec timenow;
    char sname[200];

    size_t sharedsize = 0; // shared memory size in bytes
    int SM_fd; // shared memory file descriptor
    char SM_fname[200];
    int result;
    IMAGE_METADATA *map=NULL;
    char *mapv; // pointed cast in bytes

    int kw;
//    char comment[80];
//    char kname[16];
    


    nelement = 1;
    for(i=0; i<naxis; i++)
        nelement*=size[i];

    // compute total size to be allocated
    if(shared==1)
    {
        // create semlog

        sprintf(sname, "%s_semlog", name);
        remove(sname);
        image->semlog = NULL;

        if ((image->semlog = sem_open(sname, O_CREAT, 0644, 1)) == SEM_FAILED)
            perror("semaphore creation / initilization");
        else
            sem_init(image->semlog, 1, 0);


        sharedsize = sizeof(IMAGE_METADATA);

        if(atype == _DATATYPE_UINT8)
            sharedsize += nelement*SIZEOF_DATATYPE_UINT8;
        if(atype == _DATATYPE_INT8)
            sharedsize += nelement*SIZEOF_DATATYPE_INT8;

        if(atype == _DATATYPE_UINT16)
            sharedsize += nelement*SIZEOF_DATATYPE_UINT16;
        if(atype == _DATATYPE_INT16)
            sharedsize += nelement*SIZEOF_DATATYPE_INT16;

        if(atype == _DATATYPE_INT32)
            sharedsize += nelement*SIZEOF_DATATYPE_INT32;
        if(atype == _DATATYPE_UINT32)
            sharedsize += nelement*SIZEOF_DATATYPE_UINT32;


        if(atype == _DATATYPE_INT64)
            sharedsize += nelement*SIZEOF_DATATYPE_INT64;

        if(atype == _DATATYPE_UINT64)
            sharedsize += nelement*SIZEOF_DATATYPE_UINT64;


        if(atype == _DATATYPE_FLOAT)
            sharedsize += nelement*SIZEOF_DATATYPE_FLOAT;

        if(atype == _DATATYPE_DOUBLE)
            sharedsize += nelement*SIZEOF_DATATYPE_DOUBLE;

        if(atype == _DATATYPE_COMPLEX_FLOAT)
            sharedsize += nelement*SIZEOF_DATATYPE_COMPLEX_FLOAT;

        if(atype == _DATATYPE_COMPLEX_DOUBLE)
            sharedsize += nelement*SIZEOF_DATATYPE_COMPLEX_DOUBLE;


        sharedsize += NBkw*sizeof(IMAGE_KEYWORD);


        sprintf(SM_fname, "%s/%s.im.shm", SHAREDMEMDIR, name);
        SM_fd = open(SM_fname, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600);

        if (SM_fd == -1) {
            perror("Error opening file for writing");
            exit(0);
        }




        image->shmfd = SM_fd;
        image->memsize = sharedsize;

        result = lseek(SM_fd, sharedsize-1, SEEK_SET);
        if (result == -1) {
            close(SM_fd);
            daoError("Error calling lseek() to 'stretch' the file\n");
            exit(0);
        }

        result = write(SM_fd, "", 1);
        if (result != 1) {
            close(SM_fd);
            perror("Error writing last byte of the file");
            exit(0);
        }

        map = (IMAGE_METADATA*) mmap(0, sharedsize, PROT_READ | PROT_WRITE, MAP_SHARED, SM_fd, 0);
        if (map == MAP_FAILED) {
            close(SM_fd);
            perror("Error mmapping the file");
            exit(0);
        }

        image->md = (IMAGE_METADATA*) map;
        image->md[0].shared = 1;
        image->md[0].sem = 0;
    }
    else
    {
        image->md = (IMAGE_METADATA*) malloc(sizeof(IMAGE_METADATA));
        image->md[0].shared = 0;
        if(NBkw>0)
            image->kw = (IMAGE_KEYWORD*) malloc(sizeof(IMAGE_KEYWORD)*NBkw);
        else
            image->kw = NULL;
    }

    image->md[0].atype = atype;
    image->md[0].naxis = naxis;
    strcpy(image->name, name); // local name
    strcpy(image->md[0].name, name);
    for(i=0; i<naxis; i++)
        image->md[0].size[i] = size[i];
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
            image->array.UI8 = (uint8_t*) calloc ((size_t) nelement, sizeof(uint8_t));


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
            image->array.UI16 = (uint16_t*) calloc ((size_t) nelement, sizeof(uint16_t));

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
            image->array.SI16 = (int16_t*) calloc ((size_t) nelement, sizeof(int16_t));

        if(image->array.SI16 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
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
            image->array.UI32 = (uint32_t*) calloc ((size_t) nelement, sizeof(uint32_t));

        if(image->array.UI32 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
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
            image->array.SI32 = (int32_t*) calloc ((size_t) nelement, sizeof(int32_t));

        if(image->array.SI32 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
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
            image->array.UI64 = (uint64_t*) calloc ((size_t) nelement, sizeof(uint64_t));

        if(image->array.SI64 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
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
            image->array.SI64 = (int64_t*) calloc ((size_t) nelement, sizeof(int64_t));

        if(image->array.SI64 == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
            fprintf(stderr,"\n");
            fprintf(stderr,"Requested memory size = %ld elements = %f Mb\n", (long) nelement,1.0/1024/1024*nelement*sizeof(int64_t));
            fprintf(stderr," %c[%d;m",(char) 27, 0);
            exit(0);
        }
    }


    if(atype == _DATATYPE_FLOAT)	{
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
            image->array.F = (float*) calloc ((size_t) nelement, sizeof(float));

        if(image->array.F == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
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
            image->array.D = (double*) calloc ((size_t) nelement, sizeof(double));
  
        if(image->array.D == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
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
            image->array.CF = (complex_float*) calloc ((size_t) nelement, sizeof(complex_float));

        if(image->array.CF == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
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
            image->array.CD = (complex_double*) calloc ((size_t) nelement,sizeof(complex_double));

        if(image->array.CD == NULL)
        {
            daoError("memory allocation failed\n");
            fprintf(stderr,"%c[%d;%dm", (char) 27, 1, 31);
            fprintf(stderr,"Image name = %s\n",name);
            fprintf(stderr,"Image size = ");
            fprintf(stderr,"%ld", (long) size[0]);
            for(i=1; i<naxis; i++)
                fprintf(stderr,"x%ld", (long) size[i]);
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
        daoImageCreateSem(image, 10); // by default, create 10 semaphores
    else
		image->md[0].sem = 0; // no semaphores
     


    // initialize keywords
    for(kw=0; kw<image->md[0].NBkw; kw++)
        image->kw[kw].type = 'N';


    return(0);
}

