/*
 * reader.c -- an SHM of another machine, read from C exactly as a local one.
 *
 * Nothing network-specific below: daoShmOpen finds that the SHM is not on this
 * machine and asks the local daoShmNetd, which keeps a replica with the same name.
 * The replica's keywords say where it comes from and how fresh it is.
 *
 *   daoShmNet.py start                                (on each machine)
 *   cc reader.c -o reader -I$DAOROOT/include -L$DAOROOT/lib -ldao -Wl,-rpath,$DAOROOT/lib
 *   ./reader /tmp/netDemo.im.shm                      (writer.py on the other machine)
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "dao.h"

static long long keyword(IMAGE *img, const char *name)
{
    for (int i = 0; i < img->md[0].NBkw; i++)
        if (strncmp(img->kw[i].name, name, sizeof img->kw[i].name) == 0)
            return (long long) img->kw[i].value.numl;
    return -1;
}

int main(int argc, char **argv)
{
    const char *name = argc > 1 ? argv[1] : "/tmp/netDemo.im.shm";
    IMAGE img;
    memset(&img, 0, sizeof img);                      /* libdao expects a zeroed IMAGE */
    if (daoShmOpen(name, &img) != DAO_SUCCESS) {      /* local, or brought by the service */
        fprintf(stderr, "%s: not here, nor on the network (daoShmNet.py ls)\n", name);
        return 1;
    }
    printf("%s -> %s\n", name, img.name);             /* the replica's path */
    for (int k = 0; k < 10; k++) {
        struct timespec until;
        timespec_get(&until, TIME_UTC);
        until.tv_sec += 2;
        if (daoShmWaitSemTimeout(&img, DAO_SEM_AUTO, &until) != DAO_SUCCESS) {   /* its next frame */
            printf("no frame for 2 s\n");
            continue;
        }
        printf("frame %llu: first value %g, age %lld us, link %s\n",
               (unsigned long long) daoShmGetCounter(&img), img.array.F[0],
               keyword(&img, "NET_AGE_US"), keyword(&img, "NET_LINK") == 1 ? "up" : "down");
    }
    daoShmClose(&img);
    return 0;
}
