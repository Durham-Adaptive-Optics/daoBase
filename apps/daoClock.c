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
#include "daoBase.h" 

typedef int bool_t;
#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

/*==========================================================================*/
static int	sDebug=0;						/* show debug output */
static int	sNdx=0;							/* board index */
static int	sExit=0;						/* program exit code */

//Need to install process with setuid.  Then, so you aren't running privileged all the time do this:
uid_t euid_real;
uid_t euid_called;
uid_t suid;


struct timespec tnow;
double tlastupdatedouble;

IMAGE *shm;
char clockName[32];
float frequency;

// Thread
pthread_t controllerThread;
int threadIdCtrl = 0;

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
    printf("%s of " __DATE__ " at " __TIME__ "\n",sArgv0);
    printf("   arguments:\n");
    printf("   -h               display this message and exit\n");
    printf("   -d               display program debug output\n");
    printf("   -l str           display str in output\n");
    /*
     **	Post init tests
     */
    printf("   -L Nb            real time control loop: example camsimClock -L harmoniClock 500\n");
    /*
     **	Timing tests
     */
    printf("   -t nloops        test timing for i/o\n");
    printf("\n");
}
/*--------------------------------------------------------------------------*/
static int clockRealTimeLoop(void *thread_data)
{
    int status;
    long cntloop;
    long cnt;
    long ii;
    int k;


    // MAIN LOOP
    printf("ENTERING LOOP\n");
    fflush(stdout);
    cntloop = 0;
    struct timeval t[2];
    double elapsedTime;
    double freq;
    int tcnt=0;    
    unsigned int clock[1]; 
    int frameCnt = 0;
    gettimeofday(&t[1],NULL);  
    float pauseTime;
    pauseTime = 1e6/frequency-50;
    // timing emulation there is a small offset of about 50 us...
    // probalby due to the usleep function... not very accurate.
    while (end==0) 
    {
        usleep(pauseTime);
        clock[0]++;// = clock[0] + 1;
        daoImage2ShmUI32((unsigned int*)clock, 1, &shm[0]);
        frameCnt++;
        t[0]=t[1];        
        gettimeofday(&t[1],NULL);
        elapsedTime = (t[1].tv_sec - t[0].tv_sec) * 1000.0;    // sec to ms
        elapsedTime += (t[1].tv_usec - t[0].tv_usec) / 1000.0; // us to ms
        elapsedTime = elapsedTime; // in sec... :-)
        printf("\rfps = %.3f Hz,", 1e6/(1000*elapsedTime));
        fflush(stdout);
    }

    printf("EXITING MAIN LOOP\n");
    fflush(stdout);

    return 0;
}
    
/*--------------------------------------------------------------------------*/
static int realTimeLoop()
{
    int status, k;
    // register interrupt signal to terminate the main loop
    signal(SIGINT, endme);

    shm = (IMAGE*) malloc(sizeof(IMAGE));
    daoShm2Img(clockName, "", &shm[0]);

    clock_t launch, done;
    double diff;
    launch=clock();
    status=1; 
    usleep(1000);
    done=clock();
    diff = (double)(done - launch) / CLOCKS_PER_SEC;
    printf("\n%ld, %ld, %ld\n",done, launch, CLOCKS_PER_SEC);
    printf("clock init status = %d, init time=%.3f\n", status, diff);
    fflush(stdout);

    int camThreadVal=0;
    camThreadVal = pthread_create(&controllerThread, NULL, clockRealTimeLoop, (void *)&threadIdCtrl);
    pthread_join(controllerThread, NULL);
    return 0;
}

static void DecodeArgs(int argc, char **argv)
    /*
     **	Parse the input arguments.
     */
{
    char	*str;
    int status, a1;

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
                        daoInfo("CAM real time control\n");
                        (void)sscanf(*argv++,"%s", clockName);
                        (void)sscanf(*argv++,"%f", &frequency);
                        daoInfo("%s \n", clockName);
                        daoInfo("%f \n", frequency);
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



