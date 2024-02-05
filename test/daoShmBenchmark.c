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

float nbPoints;


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
/*--------------------------------------------------------------------------*/
void * benchmarkRealTimeLoop(void *thread_data)
{
    daoInfo("ThreadId=%p\n", thread_data);
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
    daoShmImageCreate(shm10x10, '/tmp/data10x10.im.shm', 2, size, _DATATYPE_FLOAT, 1, 0);
    size[0] = 100;
    size[1] = 100;
    daoShmImageCreate(shm100x100, '/tmp/data100x100.im.shm', 2, size, _DATATYPE_FLOAT, 1, 0);
    size[0] = 1000;
    size[1] = 1000;
    daoShmImageCreate(shm1000x1000, '/tmp/data1000x1000.im.shm', 2, size, _DATATYPE_FLOAT, 1, 0);
    size[0] = 1;
    size[1] = nbPoints;
    daoShmImageCreate(shm10x0timing, '/tmp/timing10x10.im.shm', 2, size, _DATATYPE_FLOAT, 1, 0);
    daoShmImageCreate(shm100x100timing, '/tmp/timing100x100.im.shm', 2, size, _DATATYPE_FLOAT, 1, 0);
    daoShmImageCreate(shm1000x1000timing, '/tmp/timing1000x1000.im.shm', 2, size, _DATATYPE_FLOAT, 1, 0);
    // MAIN LOOP
    daoInfo("ENTERING LOOP\n");
    fflush(stdout);
    struct timespec t[3];
    double elapsedTime;
    //double shmElapsedTime=0;
    unsigned int clock[1]; 
    clock_gettime(CLOCK_REALTIME, &t[1]);
    float pauseTime;
    pauseTime = 1e6/shmFreq[0].array.F[0];
    struct timespec tc;
    clock_gettime(CLOCK_MONOTONIC, &tc);

    for (count=0; count<bPoints; k++)
    {

    }
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
    daoInfo('This benchmark is measuring the write time into the shared memory for different data size\n');
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



