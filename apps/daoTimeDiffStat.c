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

double dt_update; // time since last update
double dt_update_lim = 3600.0; // if no command is received during this time, set DM to zero V [sec]

IMAGE *shm;
IMAGE *shmAvg;
IMAGE *shmRms;

char shmName[32];
char shmNameAvg[32];
char shmNameRms[32];
int popSize;

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
    daoInfo("%s of " __DATE__ " at " __TIME__ "\n",sArgv0);
    daoInfo("   arguments:\n");
    daoInfo("   -h               display this message and exit\n");
    daoInfo("   -d               display program debug output\n");
    /*
     **	Post init tests
     */
    daoInfo("   -L               real time control loop\n");
    daoInfo("   -s str           shared memory name to average\n");
    daoInfo("   -t nloops        number of frame to average\n");
    daoInfo("\n");
}

/*--------------------------------------------------------------------------*/
static int realTimeLoop()
{
    // register interrupt signal to terminate the main loop
    signal(SIGINT, endme);

    daoInfo("Starting loop, %s -> %s, popSize=%d\n",shmName, shmNameAvg, popSize );
    daoInfo("               %s -> %s, popSize=%d\n",shmName, shmNameRms, popSize );
    fflush(stdout);
    shm = (IMAGE*) malloc(sizeof(IMAGE));
    shmAvg = (IMAGE*) malloc(sizeof(IMAGE));
    shmRms = (IMAGE*) malloc(sizeof(IMAGE));
    daoShm2Img(shmName, "", &shm[0]);
    daoShm2Img(shmNameAvg, "", &shmAvg[0]);
    daoShm2Img(shmNameRms, "", &shmRms[0]);

    int nbValue = shm[0].md[0].size[0]*shm[0].md[0].size[1];
    float *avgValue = malloc(nbValue*sizeof(float));
    float *rmsValue = malloc(nbValue*sizeof(float));
    float valueCircBufAvg[nbValue][popSize+1];
    float valueCircBuf[nbValue][popSize+1];
    int k, l;
    // reset buffer
    for (k=0; k<nbValue; k++)
    {
        for (l=0; l<(popSize + 1); l++)
        {
            valueCircBufAvg[k][l] = 0.0;
            valueCircBuf[k][l] = 0.0;
        }
        avgValue[k] = 0.0;
        rmsValue[k] = 0.0;
    }
    int tail=0;
    int head=0;
    int c=0;
    daoInfo("Average telemetry running for %s -> %s, popSize=%d\n",shmName, shmNameAvg, popSize );

    struct timespec timeout;

    while (end ==0)
    {
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1; // 1 second timeout
        // Wait for new image
        if (sem_timedwait(shm[0].semptr[9], &timeout) != -1)
        {
            // if new image, add it in the cir buf.
            for (k = 0; k < nbValue; k++)
            {
                valueCircBufAvg[k][tail] = shm[0].array.F[k] / popSize;
                valueCircBuf[k][tail] = shm[0].array.F[k];
                if (isnan(valueCircBufAvg[k][tail]))
                {
                    valueCircBufAvg[k][tail] = 0.0;
                    valueCircBuf[k][tail] = 0.0;
                }
            }
            tail = (tail + 1) % (popSize + 1);
            for (k = 0; k < nbValue; k++)
            {
                if (!isnan(valueCircBufAvg[k][head]))
                {
                    // add value in the head
                    avgValue[k] += valueCircBufAvg[k][head];
                }
                else
                {
                    // add value in the head
                    avgValue[k] += 0.0;
                }
                // remove the tail value
                avgValue[k] -= valueCircBufAvg[k][tail];
            }
            for (k = 0; k < nbValue; k++)
            {
                for (c = 0; c < popSize; c++)
                {
                    if (!isnan(valueCircBuf[k][c]))
                    {
                        // add value in the c
                        rmsValue[k] += pow(valueCircBuf[k][c] - avgValue[k], 2);
                    }
                    else
                    {
                        // add value in the head
                        rmsValue[k] += 0.0;
                    }
                }
                rmsValue[k] = sqrt(rmsValue[k] / popSize);
            }
            head = (head + 1) % (popSize + 1);

            daoImage2Shm(avgValue, nbValue, &shmAvg[0]);
            daoImage2Shm(rmsValue, nbValue, &shmRms[0]);
            printf("\r(%.3f,%.3f) -> AVG(%.3f,%.3f), RMS(%.3f,%.3f)",
                   shm[0].array.F[0], shm[0].array.F[1],
                   shmAvg[0].array.F[0], shmAvg[0].array.F[1],
                   shmRms[0].array.F[0], shmRms[0].array.F[1]);
            fflush(stdout);
        }
    }

    daoInfo("EXITING MAIN LOOP\n");
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
            case 's':	
                        (void)sscanf(*argv++,"%s",shmName); argc -= 1;
                        sprintf(shmNameAvg,"%sAvg", shmName);
                        sprintf(shmNameRms,"%sRms", shmName);
                        daoInfo("SHM = %s\n", shmName);
                        break;
            case 't':	(void)sscanf(*argv++,"%d",&popSize); argc -= 1;	break;
            case 'L':
                        daoInfo("Average Telemetry real time control\n");
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

