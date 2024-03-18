/*****************************************************************************
  DAO project
  s.cetre
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/file.h>
#include <errno.h>
#include <sys/mman.h>
#include <sched.h>
#include <semaphore.h>
#include <sys/time.h>
#include <termios.h>

#include <pthread.h>

// DAO header
#include "dao.h" 

typedef int bool_t;
#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

/*==========================================================================*/
static int	sNdx=0;							/* board index */
static int	sExit=0;						/* program exit code */

//Need to install process with setuid.  Then, so you aren't running privileged all the time do this:
uid_t euid_real;
uid_t euid_called;
uid_t suid;

struct timespec tnow;
double tlastupdatedouble;

int nbPoints;


// termination flag
static int end     = 0;

// termination function for SIGINT callback
static void endme() 
{
    end = 1;
}

static char	*sArgv0=NULL;					/* name of executable */

static void ShowHelp(void)
{
    daoInfo("%s of " __DATE__ " at " __TIME__ "\n",sArgv0);
    daoInfo("   arguments:\n");
    daoInfo("   -h               display this message and exit\n");
    daoInfo("   -d               display program debug output\n");
    daoInfo("   -l str           display str in output\n");
    /*
     **	Post init tests
     */
    daoInfo("   -L Nb            real time control loop: example camsimClock -L harmoniClock 500\n");
    /*
     **	Timing tests
     */
    daoInfo("   -t nloops        test timing for i/o\n");
    daoInfo("\n");
}

// Function to calculate the mean of the vector
double avg(double *vector, int size) 
{
    double sum = 0.0;
    for (int i = 0; i < size; i++) 
    {
        sum += vector[i];
    }
    return sum / size;
}

// Function to calculate the standard deviation of the vector
double std(double *vector, int size) 
{
    double mean = avg(vector, size);
    double sum = 0.0;

    for (int i = 0; i < size; i++) 
    {
        sum += pow(vector[i] - mean, 2);
    }

    return sqrt(sum / size);
}

/*--------------------------------------------------------------------------*/
void * benchmarkRealTimeLoop(void *thread_data)
{
    daoInfo("ThreadId=%p\n", thread_data);
    int count;
    IMAGE *shm10x10 = (IMAGE*) malloc(sizeof(IMAGE));
    IMAGE *shm100x100 = (IMAGE*) malloc(sizeof(IMAGE));
    IMAGE *shm1000x1000 = (IMAGE*) malloc(sizeof(IMAGE));
    IMAGE *shm10x10timing = (IMAGE*) malloc(sizeof(IMAGE));
    IMAGE *shm100x100timing = (IMAGE*) malloc(sizeof(IMAGE));
    IMAGE *shm1000x1000timing = (IMAGE*) malloc(sizeof(IMAGE));
    // Create size array, using 2D of 1x1... can be change to 1D
    uint32_t size[2];
    // Create SHM
    size[0] = 10;
    size[1] = 10;
    daoShmImageCreate(shm10x10, "/tmp/data10x10.im.shm", 2, size, _DATATYPE_DOUBLE, 1, 0);
    size[0] = 100;
    size[1] = 100;
    daoShmImageCreate(shm100x100, "/tmp/data100x100.im.shm", 2, size, _DATATYPE_FLOAT, 1, 0);
    size[0] = 1000;
    size[1] = 1000;
    daoShmImageCreate(shm1000x1000, "/tmp/data1000x1000.im.shm", 2, size, _DATATYPE_FLOAT, 1, 0);
    size[0] = 1;
    size[1] = nbPoints;
    daoShmImageCreate(shm10x10timing, "/tmp/timing10x10.im.shm", 2, size, _DATATYPE_DOUBLE, 1, 0);
    daoShmImageCreate(shm100x100timing, "/tmp/timing100x100.im.shm", 2, size, _DATATYPE_DOUBLE, 1, 0);
    daoShmImageCreate(shm1000x1000timing, "/tmp/timing1000x1000.im.shm", 2, size, _DATATYPE_DOUBLE, 1, 0);
    // Create random array of 
    double array10x10[10*10];
    double array100x100[100*100];
    double array1000x1000[1000*1000];
    // MAIN LOOP
    daoInfo("ENTERING LOOP\n");
    fflush(stdout);
    struct timespec t[3];
//    double elapsedTime[nbPoints];
    //double shmElapsedTime=0;
    unsigned int clock[1]; 
    clock_gettime(CLOCK_REALTIME, &t[1]);
    struct timespec tc;
    clock_gettime(CLOCK_MONOTONIC, &tc);

    daoInfo("Testing 10x10 write\n");
    // Warmup
    for (count=0; count<10000; count++)
    {
        daoShmImage2Shm((double*)array10x10, 1e2, &shm10x10[0]);
    }    
    for (count=0; count<nbPoints; count++)
    {
        clock_gettime(CLOCK_REALTIME, &t[0]);
        daoShmImage2Shm((double*)array10x10, 1e2, &shm10x10[0]);
        clock_gettime(CLOCK_REALTIME, &t[1]);
        shm10x10timing[0].array.D[count] = (t[1].tv_sec - t[0].tv_sec) * 1e3;    // sec to ms
        shm10x10timing[0].array.D[count] += (t[1].tv_nsec - t[0].tv_nsec) / 1e6; // us to ms
    }
    daoInfo("Average time: %lf\n", avg(shm10x10timing[0].array.D, nbPoints));
    daoInfo("Jitter      : %lf\n", std(shm10x10timing[0].array.D, nbPoints));
    daoInfo("Testing 100x100 write\n");
    // Warmup
    for (count=0; count<10000; count++)
    {
        daoShmImage2Shm((double*)array100x100, 1e2, &shm100x100[0]);
    }    
    for (count=0; count<nbPoints; count++)
    {
        clock_gettime(CLOCK_REALTIME, &t[0]);
        daoShmImage2Shm((double*)array100x100, 1e2, &shm100x100[0]);
        clock_gettime(CLOCK_REALTIME, &t[1]);
        shm100x100timing[0].array.D[count] = (t[1].tv_sec - t[0].tv_sec) * 1e3;    // sec to ms
        shm100x100timing[0].array.D[count] += (t[1].tv_nsec - t[0].tv_nsec) / 1e6; // us to ms
    }
    daoInfo("Average time: %lf\n", avg(shm100x100timing[0].array.D, nbPoints));
    daoInfo("Jitter      : %lf\n", std(shm100x100timing[0].array.D, nbPoints));
    daoInfo("Testing 1000x1000 write\n");
    // Warmup
    for (count=0; count<10000; count++)
    {
        daoShmImage2Shm((double*)array1000x1000, 1e2, &shm1000x1000[0]);
    }    
    for (count=0; count<nbPoints; count++)
    {
        clock_gettime(CLOCK_REALTIME, &t[0]);
        daoShmImage2Shm((double*)array1000x1000, 1e2, &shm1000x1000[0]);
        clock_gettime(CLOCK_REALTIME, &t[1]);
        shm1000x1000timing[0].array.D[count] = (t[1].tv_sec - t[0].tv_sec) * 1e3;    // sec to ms
        shm1000x1000timing[0].array.D[count] += (t[1].tv_nsec - t[0].tv_nsec) / 1e6; // us to ms
    }
    daoInfo("Average time: %lf\n", avg(shm1000x1000timing[0].array.D, nbPoints));
    daoInfo("Jitter      : %lf\n", std(shm1000x1000timing[0].array.D, nbPoints));
    daoInfo("EXITING MAIN LOOP\n");
    fflush(stdout);

    return DAO_SUCCESS;
}
    
/*--------------------------------------------------------------------------*/
static int realTimeLoop()
{
    int status;
    // register interrupt signal to terminate the main loop
    signal(SIGINT, endme);
    
    fflush(stdout);
    daoInfo("This benchmark is measuring the write time into the shared memory for different data size\n");
    daoInfo("10x10     image\n");
    daoInfo("100x100   image\n");
    daoInfo("1000x1000 image\n");
    // Thread
    pthread_t benchmarkThread;
    int threadIdCtrl = 0;
    int benchmarkThreadVal=0;
    benchmarkThreadVal = pthread_create(&benchmarkThread, NULL, benchmarkRealTimeLoop, (void *)&threadIdCtrl);
    if (benchmarkThreadVal != 0)
    {
        daoError("Cannot create thread, err\n");
        return DAO_ERROR;
    }
    pthread_join(benchmarkThread, NULL);
    return DAO_SUCCESS;
}

static void DecodeArgs(int argc, char **argv)
    /*
     **	Parse the input arguments.
     */
{
    char	*str;
    int a1;

    argv += 1;	argc -= 1;					/* skip program name */

    while (argc-- > 0) {
        daoDebug("DecodeArgs: working on '%s'/%d\n",*argv,argc);
        str = *argv++;
        if (str[0] != '-') {
            daoError("Do not know arg '%s'\n",str);
            ShowHelp();
            exit(1);
        }

        switch (str[1]) {
            case 'h':	ShowHelp(); exit(0);
	    case 'd':	
			(void)sscanf(*argv++,"%d",&daoLogLevel); argc -= 1;
			break;
            case 'l':
                        daoInfo("%s\n",*argv);
                        argv += 1; argc -= 1;
                        break;

            case 'b':	(void)sscanf(*argv++,"%d",&sNdx); argc -= 1;	break;
            case 'u':
                        (void)sscanf(*argv++,"%d",&a1); argc -= 1;
                        daoDebug("will sleep for %d usec\n",a1);
                        (void)usleep(a1);
                        break;
            case 'L':
                        daoInfo("Clock real time control\n");
                        (void)sscanf(*argv++,"%d", &nbPoints);
                        daoInfo("number of points = %d \n", nbPoints);
                        realTimeLoop();
                        break;
            default:
                        daoError("Do not know arg '%s'\n",str);
                        ShowHelp();
                        exit(2);
        }
    }

    return;
}
/*==========================================================================*/
int main(int argc, char **argv)
    /*
     **	Fetch the arguments and do what is requested
     */
{
    int RT_priority = 93; //any number from 0-99
    struct sched_param schedpar;

    schedpar.sched_priority = RT_priority;
    // r = seteuid(euid_called); //This goes up to maximum privileges
    sched_setscheduler(0, SCHED_FIFO, &schedpar); //other option is SCHED_RR, might be faster
    // r = seteuid(euid_real);//Go back to normal privileges

    sArgv0 = *argv;

    DecodeArgs(argc,argv);

    return(sExit);
}
/*==========================================================================*/



