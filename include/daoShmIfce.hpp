#ifndef DAO_SHM_IFCE_H
#define DAO_SHM_IFCE_H

#include "daoImageStruct.h"


#include <unistd.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <numa.h>
#include <numaif.h>

// C++ verison of some of the stuff
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <type_traits>
#include <time.h>

template<class T>
class DaoShmIfce {
    public:
        DaoShmIfce()
        : shm(0)
        , map(0)
        , shm_filename("")
        , shm_file(0)
        , shm_filesize(0)
        , shm_nElements(0)
        , mapv(0)
        , atype(0)
        , file_is_open(false)
        , time()
        , timeout_small_us(10000)
        , timeout_large_us(10000000)
        , timeout_ratio(timeout_large_us / timeout_small_us)
        {
            printf("DaoShmIfce()\n");
            if (clock_gettime(CLOCK_REALTIME, &time) == -1)
            {
                perror("clock_gettime");
                exit(EXIT_FAILURE);
            }
            timeout_ratio = timeout_large_us/timeout_small_us;
        }

        ~DaoShmIfce()
        {
            printf(" ~DaoShmIfce()\n");
            if(file_is_open)
            {
                closeShm();
            }
        }

        void init()
        {
            if(file_is_open)
            {
                closeShm();
            }

        }

        void openShm(std::string name, IMAGE * shm_p, int node = -1)
        {
            printf("openShm(%s, - )\n", name.c_str());
            if(file_is_open)
            {
                closeShm();
                printf("Error: File %s already open. Closing shm and retrying\n", name.c_str());
                openShm(name, shm_p);
            }
            
            shm = shm_p;
            shm_filename = name;
   
            shm_file = open(shm_filename.c_str(), O_RDWR);
            if(shm_file == -1)
            {
                shm->used = 0;
                printf("Cannot import shared memory file %s\n", shm_filename.c_str());
                closeShm();
                return;
            }
            else
            {
                file_is_open = true;
                struct stat file_stat;
                fstat(shm_file, &file_stat);
                shm_filesize = file_stat.st_size;

                printf("File %s size: %zd\n", shm_filename.c_str(), shm_filesize);
                map = (IMAGE_METADATA*) mmap(0, shm_filesize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_file, 0);
                if(node >= 0)
                {
                    int numa_node = -1;
                    get_mempolicy(&numa_node, NULL, 0, (void*)map, MPOL_F_NODE | MPOL_F_ADDR);
                    printf("Memory currently on numa node: %d\n", numa_node);
                    printf("Moving memory to node %d\n", node);
                    numa_tonode_memory((void*) map, shm_filesize, node);
                    get_mempolicy(&numa_node, NULL, 0, (void*)map, MPOL_F_NODE | MPOL_F_ADDR);
                    printf("Memory now on numa node: %d\n", numa_node);
                }
                else
                {
                    printf("node: %d\n", node);
                }

                printf("Maping done\n");
                if (map == MAP_FAILED) 
                {
                    closeShm();
                    printf("Error: mmapping the file\n");
                    return;
                }

                shm->memsize = shm_filesize;
                shm->shmfd = shm_file;
                shm->md = map;
                atype = shm->md[0].atype;
                shm->md[0].shared = 1;
                shm_nElements = shm->md[0].size[0]* shm->md[0].size[1];

                printf("image size = %ld x %ld: %ld\n", (long) shm->md[0].size[0], (long) shm->md[0].size[1], shm_nElements);
                
                if(shm->md[0].size[0]<1)
                {
                    printf("IMAGE \"%s\" AXIS SIZE < 1... ABORTING\n", shm_filename.c_str());
                    closeShm();
                    return;
                }
                if(shm->md[0].size[1]<1)
                {
                    printf("IMAGE \"%s\" AXIS SIZE < 1... ABORTING\n", shm_filename.c_str());
                    closeShm();
                    return;

                }

                mapv = (char*) map;
                mapv += sizeof(IMAGE_METADATA);

                printf("atype = %d\n", (int) atype);
                // hack due to using void pointer.
                shm->array.V = (void *) mapv;
                mapv += sizeof(T) * shm->md[0].nelement;

                int kw;
                shm->kw = (IMAGE_KEYWORD*) (mapv);
                for(kw=0; kw<shm->md[0].NBkw; kw++)
                {
                    if(shm->kw[kw].type == 'L')
                        printf("%d  %s %ld %s\n", kw, shm->kw[kw].name, shm->kw[kw].value.numl, shm->kw[kw].comment);
                    if(shm->kw[kw].type == 'D')
                        printf("%d  %s %lf %s\n", kw, shm->kw[kw].name, shm->kw[kw].value.numf, shm->kw[kw].comment);
                    if(shm->kw[kw].type == 'S')
                        printf("%d  %s %s %s\n", kw, shm->kw[kw].name, shm->kw[kw].value.valstr, shm->kw[kw].comment);
                }

                mapv += sizeof(IMAGE_KEYWORD)*shm->md[0].NBkw;
                strcpy(shm->name, shm_filename.c_str());
                // looking for semaphores
                int sem_number = 0;
                bool sem_OK = false;
                sem_t *stest;
                while(!sem_OK)
                {
                    std::ostringstream sem_name;
                    sem_name << shm->md[0].name << "_sem" << std::setw(2) << std::setfill('0') << sem_number;
                    printf("semaphore %s\n", sem_name.str().c_str());
                    
                    if((stest = sem_open(sem_name.str().c_str(), 0, 0644, 0)) == SEM_FAILED)
                    {
                        sem_OK = true;
                    }
                    else
                    {
                        sem_close(stest);
                        sem_number++;
                    }
                    
                }
                
                printf("%d semaphores detected  (shm->md[0].sem = %d)\n", sem_number, (int) shm->md[0].sem);
                shm->semptr = (sem_t**) malloc(sizeof(sem_t*) * shm->md[0].sem);
                for(int i = 0; i < sem_number; i++)
                {
                    std::ostringstream sem_name;
                    sem_name << shm->md[0].name << "_sem" << std::setw(2) << std::setfill('0') << i;
                    if ((shm->semptr[i] = sem_open(sem_name.str().c_str(), 0, 0644, 0))== SEM_FAILED) 
                    {
                        printf("could not open semaphore %s\n", sem_name.str().c_str());
                    }
                }
                
                std::ostringstream sem_logname;
                // sprintf(sname, "%s_semlog", image->md[0].name);
                sem_logname << shm->md[0].name << "_semlog\n";
                if ((shm->semlog = sem_open(sem_logname.str().c_str(), 0, 0644, 0))== SEM_FAILED) 
                {
                    printf("could not open semaphore %s\n", sem_logname.str().c_str());
                }         
            }




        }

        void closeShm()
        {
            if(file_is_open)
            {
                // unmap memory
                printf("Unmapping shm memory \n");
                munmap(map, shm_filesize);

                // close file
                printf("Closing file - %s\n", shm_filename.c_str());
                close(shm_file);

                // set file is closed set all variables to default.
                file_is_open = false;
                reset();
            }
            else
            {
                printf("Error: No file open cannot close %s\n", shm_filename.c_str());
            }
        }

        void getFrame()
        {
            if(file_is_open)
            {
                sem_wait(shm[0].semptr[1]);
            }
            else
            {
                printf("Error: No file open cannot wait for data\n");
            }
        }

        int getFrame(volatile bool* system_running)
        {
            if(file_is_open)
            {
                size_t i = 0;
                while(true)
                {
                    if (clock_gettime(CLOCK_REALTIME, &time) == -1) 
                    {
                        perror("clock_gettime");
                        exit(EXIT_FAILURE);
                    }
                    time.tv_nsec +=(int) timeout_small_us*1000;
                    if (time.tv_nsec>=1000000000)
                    {
                        time.tv_sec += time.tv_nsec / 1000000000;
                        time.tv_nsec %= 1000000000;
                    }
                    int rVal = sem_timedwait(shm[0].semptr[1], &time);
                    if(rVal == 0)
                    {
                        return 0;
                    }
                    if(!system_running)
                    {
                        printf("exit requested\n");
                        return -1;
                    }
                    if (i >= timeout_ratio)
                    {
                        // printf("i: %zu, timeout_ratio: %zu\n", i, timeout_ratio);
                        return -10;
                    }
                    i = i + 1;
                }
            }
            else
            {
                printf("Error: No file open cannot wait for data\n");
            }
            return -1;
        }

        T * getPtr()
        {
            if(file_is_open)
            {
                return reinterpret_cast<T*>(shm->array.F);
            }

            printf("Error: No file open cannot close %s\n", shm_filename.c_str());
            return NULL;
        }

        void writeData(T * data)
        {
            int semval = 0;
            shm[0].md[0].write = 1;
            T* dst = getPtr();

            // copy data in.
            memcpy(dst,data, shm_nElements*sizeof(T));

            for(int i = 0; i < shm[0].md[0].sem; i++)
            {
                sem_getvalue(shm[0].semptr[i], &semval);
                if(semval < SEMAPHORE_MAXVAL )
                    sem_post(shm[0].semptr[i]);
            }

            if(shm[0].semlog != NULL)
            {
                sem_getvalue(shm[0].semlog, &semval);
                if(semval < SEMAPHORE_MAXVAL)
                {
                    sem_post(shm[0].semlog);
                }
            }

            shm[0].md[0].write = 0;
            shm[0].md[0].cnt0++;
        }

        void writeData()
        {
            int semval = 0;
            shm[0].md[0].write = 1;

            for(int i = 0; i < shm[0].md[0].sem; i++)
            {
                sem_getvalue(shm[0].semptr[i], &semval);
                if(semval < SEMAPHORE_MAXVAL )
                    sem_post(shm[0].semptr[i]);
            }

            if(shm[0].semlog != NULL)
            {
                sem_getvalue(shm[0].semlog, &semval);
                if(semval < SEMAPHORE_MAXVAL)
                {
                    sem_post(shm[0].semlog);
                }
            }

            shm[0].md[0].write = 0;
            shm[0].md[0].cnt0++;
        }


        void writeData(int32_t counter)
        {
            int semval = 0;
            shm[0].md[0].write = 1;
            shm[0].md[0].cnt2 = counter;

            for(int i = 0; i < shm[0].md[0].sem; i++)
            {
                sem_getvalue(shm[0].semptr[i], &semval);
                if(semval < SEMAPHORE_MAXVAL )
                    sem_post(shm[0].semptr[i]);
            }

            if(shm[0].semlog != NULL)
            {
                sem_getvalue(shm[0].semlog, &semval);
                if(semval < SEMAPHORE_MAXVAL)
                {
                    sem_post(shm[0].semlog);
                }
            }

            shm[0].md[0].write = 0;
            shm[0].md[0].cnt0++;

            struct timespec t;
            clock_gettime(CLOCK_REALTIME, &t);
            shm[0].md[0].atime.tsfixed.secondlong = (unsigned long)(1e9 * t.tv_sec + t.tv_nsec);
        }


        int32_t getCounter()
        {
            return shm[0].md[0].cnt2;
        }
    
        size_t size(){return shm_nElements;}


        void releaseAllSemaphores()
        {
            int semval = 0;
            for(int i = 0; i < shm[0].md[0].sem; i++)
            {
                sem_getvalue(shm[0].semptr[i], &semval);
                if(semval < SEMAPHORE_MAXVAL )
                    sem_post(shm[0].semptr[i]);
            }

            if(shm[0].semlog != NULL)
            {
                sem_getvalue(shm[0].semlog, &semval);
                if(semval < SEMAPHORE_MAXVAL)
                {
                    sem_post(shm[0].semlog);
                }
            }
        }

        void clean_sems()
        {
            printf("Cleaning semaphores to begin processing");
            int retval = 0;
            for(int i = 0; i < shm[0].md[0].sem; i++)
            {
                printf("cleaning semaphore %d \n", i);
                while(retval>=0)
                {
                    retval = sem_trywait(shm[0].semptr[i]);
                }
                printf("rSemaphore %d clean\n", i);
                retval = 0;
            }
        }

    protected:

        void reset()
        {
            if(file_is_open)
            {
                closeShm();
            }

            map = 0;
            shm_filename = "";
            shm_filesize = 0;
            shm_nElements = 0;
            mapv = 0;
            atype = 0;
            file_is_open = false;
            timeout_small_us = 10000;
            timeout_large_us = 10000000;
            timeout_ratio = timeout_large_us / timeout_small_us;

            if (clock_gettime(CLOCK_REALTIME, &time) == -1)
            {
                perror("clock_gettime");
                exit(EXIT_FAILURE);
            }
        }

        IMAGE * shm;
        IMAGE_METADATA *map;

        std::string shm_filename;

        int shm_file;

        size_t shm_filesize;
        size_t shm_nElements;

        char *mapv;
        uint8_t atype;
        bool file_is_open;

        struct timespec time;
        size_t timeout_small_us;
        size_t timeout_large_us;
        size_t timeout_ratio;

};

#endif