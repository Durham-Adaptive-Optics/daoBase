#include <dao.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void testCreation()
{
    char* name = "/tmp/sciImageGain.im.shm";
    uint32_t size[2];
    size[0] = 1;
    size[1] = 1;

    IMAGE* img = (IMAGE*)malloc(sizeof(IMAGE));
    long naxis = 2;
    uint8_t atype = 9; // float
    int shared = 1;
    int NBkw = 0;
    daoShmImageCreate(img, name, naxis, size, atype, shared, NBkw);
    // free(img);
}

// test for
int main()
{
    daoLogSetLevel(4);
    int i = 0;
    while (1)
    {
        long mem = 1;
        printf("Testing iteration: %d  usage: %ld\n", i, mem);
        testCreation();
        i++;
        usleep(100);
    }

    return 0;
}
