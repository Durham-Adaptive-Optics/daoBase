/*****************************************************************************
  Durham AO project
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

IMAGE *outShm;
IMAGE *clockShm;

char outShmName[32];
char clockShmName[32];

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
    printf("   -L shm clockShm real time control loop\n");
    printf("\n");
}

/*--------------------------------------------------------------------------*/
static int realTimeLoop()
{
    // register interrupt signal to terminate the main loop
    signal(SIGINT, endme);

    daoInfo("Starting loop, %s \n", outShmName);
    fflush(stdout);
    outShm = (IMAGE*) malloc(sizeof(IMAGE));
    daoShm2Img(outShmName, "", &outShm[0]);
    clockShm = (IMAGE*) malloc(sizeof(IMAGE));
    daoShm2Img(clockShmName, "", &clockShm[0]);

    int outSize = outShm[0].md[0].size[0]*outShm[0].md[0].size[1];
    struct timespec t[4];
    double elapsedTime;
    clock_gettime(CLOCK_REALTIME, &t[1]);
    int k;
    float outCmd[outSize];
    // time out for semaphore wait
    struct timespec timeout;
    timeout.tv_sec = 1; // 1 second timeout
    while (end ==0)
    {
        // Wait for the clock frame using semaphore
        if (sem_timedwait(clockShm[0].semptr[0], &timeout) != -1)
        {
            t[0] = t[1];
            outShm[0].md[0].cnt2 = outShm[0].md[0].cnt2 + 1;
            clock_gettime(CLOCK_REALTIME, &t[2]);
            for (k = 0; k < outSize; k++)
            {
                outCmd[k] = 1 - 2 * (float)rand() / (float)RAND_MAX;
            }

            daoImage2Shm((float *)outCmd, outSize, &outShm[0]);

            clock_gettime(CLOCK_REALTIME, &t[1]);
            elapsedTime = (t[1].tv_sec - t[0].tv_sec) * 1e3;
            elapsedTime += (t[1].tv_nsec - t[0].tv_nsec) / 1e6;
            printf("\rfps = %.3f Hz, out[%6.3f, %6.3f,...,%6.3f]", 1e6 / (1000 * elapsedTime),
                   outShm[0].array.F[0],
                   outShm[0].array.F[1],
                   outShm[0].array.F[outSize - 1]);
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
                        printf("Simple writer from SHM real time control\n");
                    	(void)sscanf(*argv++,"%s",outShmName); argc -= 1;
                        (void)sscanf(*argv++,"%s",clockShmName); argc -= 1;
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

