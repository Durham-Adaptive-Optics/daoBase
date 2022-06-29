/*****************************************************************************
  Pyramid WFS project
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
#include <pthread.h>

#include "daoBase.h"

/*==========================================================================*/
static int	sNdx=0;							/* board index */
static int	sExit=0;						/* program exit code */

//Need to install process with setuid.  Then, so you aren't running privileged all the time do this:
uid_t euid_real;
uid_t euid_called;
uid_t suid;

struct timespec tnow;
double tnowdouble;
double tlastupdatedouble;

IMAGE *shm0;
IMAGE *shm1;
IMAGE *latencyShm;

char shm0Name[32];
char shm1Name[32];
char latencyShmName[32];
int sem0;
int sem1;

static int   		end     = 0;		           // termination flag
// termination function for SIGINT callback
static void endme()
{
    end = 1;
}

/*--------------------------------------------------------------------------*/
static char	*sArgv0=NULL;					/* name of executable */

static void ShowHelp(void)
{
    printf("%s of " __DATE__ " at " __TIME__ "\n",sArgv0);
    printf("   arguments:\n");
    printf("   -h               display this message and exit\n");
    printf("   -d               display program debug output\n");
    /*
     **	Post init tests
     */
    printf("   -L sham              real time control loop\n");
    printf("\n");
}

/*--------------------------------------------------------------------------*/
static int realTimeLoop()
{
    // register interrupt signal to terminate the main loop
    signal(SIGINT, endme);

    daoInfo("Starting loop, %s/%s \n",shm0Name, shm1Name);
    fflush(stdout);
    shm0 = (IMAGE*) malloc(sizeof(IMAGE));
    shm1 = (IMAGE*) malloc(sizeof(IMAGE));
    latencyShm = (IMAGE*) malloc(sizeof(IMAGE));
    daoShm2Img(shm0Name, "", &shm0[0]);
    daoShm2Img(shm1Name, "", &shm1[0]);
    daoShm2Img(latencyShmName, "", &latencyShm[0]);

    int64_t t[2];
    int64_t elapsedTimeNs;
    int64_t frameId0;
    int64_t frameId1;
    int64_t frameIdDiff;
    int last = 0;
    float latency[1];
    struct timespec timeout;
    while (end ==0)
    {
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1; // 1 second timeout
        // Wait for new image
        if (sem_timedwait(shm0[0].semptr[sem0], &timeout) != -1)
        {
            //sem_wait(shm0[0].semptr[sem0]);
            t[0] = shm0[0].md[0].atime.tsfixed.secondlong;
            frameId0 = shm0[0].md[0].cnt2;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 1; // 1 second timeout
            // wait for 2nd shm
            if (sem_timedwait(shm1[0].semptr[sem1], &timeout) != -1)
            {
                // sem_wait(shm1[0].semptr[sem1]);
                t[1] = shm1[0].md[0].atime.tsfixed.secondlong;
                frameId1 = shm1[0].md[0].cnt2;
                // t[0] = shm0[0].md[0].atime.tsfixed.secondlong;
                // frameId0 = shm0[0].md[0].cnt2;
                // Check if timeout
                elapsedTimeNs = t[1] - t[0];
                latency[0] = (float)elapsedTimeNs / 1e3;
                frameIdDiff = frameId1-frameId0;
                if (frameId0 == frameId1)
                {
                    daoImage2Shm((float *)latency, 1, &latencyShm[0]);
                    if (last == 0)
                    {
                        printf("\n");
                    }
                    printf("\r    sync frame %ld %ld delta = %ld, %12ld-%12ld=%ld -> dt = %.3f us",
                           frameId0, frameId1, frameIdDiff, t[0], t[1], elapsedTimeNs, (double)elapsedTimeNs / 1e3);
                    last = 1;
                }
                else
                {
                    if (last == 1)
                    {
                        printf("\n");
                    }
                    printf("\r de-sync frame %ld %ld, delta = %ld                 -> dt = %.3f us",
                           frameId0, frameId1, frameIdDiff, (double)elapsedTimeNs / 1e3);
                    // try to resync
                    if (frameIdDiff > 0)
                    {

                        clock_gettime(CLOCK_REALTIME, &timeout);
                        timeout.tv_sec += 1; // 1 second timeout
                        // Wait for new image
                        if (sem_timedwait(shm0[0].semptr[sem0], &timeout) == -1)
                        {
                            printf("Cannot resync input...");
                        }
                    }
                    else
                    {
                        clock_gettime(CLOCK_REALTIME, &timeout);
                        timeout.tv_sec += 1; // 1 second timeout
                        // Wait for new image
                        if (sem_timedwait(shm1[0].semptr[sem1], &timeout) == -1)
                        {
                            printf("Cannot resync ouput...");
                        }
                        
                    }
                    last = 0;
                }
                fflush(stdout);
            }
            else
            {
                printf("\rTimeout: shm1, error:%s", strerror(errno));
                fflush(stdout);
            }
        }
        else
        {
            printf("\rTimeout: shm0, error:%s", strerror(errno));
            fflush(stdout);
        }
    }

    printf("EXITING MAIN LOOP\n");
    fflush(stdout);

    return 0;
}

/**
 *	Parse the input arguments.
 */
static void DecodeArgs(int argc, char **argv)
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
                        break;
            case 'L':
                        printf("Simple Camera Reader and Writer from SHM real time control\n");
                    	(void)sscanf(*argv++,"%s",shm0Name); argc -= 1;
                    	(void)sscanf(*argv++,"%s",shm1Name); argc -= 1;
                    	(void)sscanf(*argv++,"%d",&sem0); argc -= 1;
                    	(void)sscanf(*argv++,"%d",&sem1); argc -= 1;
                    	(void)sscanf(*argv++,"%s",latencyShmName); argc -= 1;
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

