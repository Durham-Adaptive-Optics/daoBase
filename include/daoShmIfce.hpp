#ifndef DAO_SHM_IFCE_H
#define DAO_SHM_IFCE_H

#include "daoImageStruct.h"
#include <daoLog.hpp>


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

namespace Dao
{
    template<class T>
    class ShmIfce {
        public:
            ShmIfce(Log::Logger& logger)
            : m_shm(0)
            , m_map(0)
            , m_ptr(0)
            , m_log(logger)
            , m_shm_filename("")
            , m_shm_file(0)
            , m_shm_filesize(0)
            , m_shm_nElements(0)
            , m_mapv(0)
            , m_atype(0)
            , m_file_is_open(false)
            , m_time()
            , m_timeout_small_us(10000)
            , m_timeout_large_us(10000000)
            , m_timeout_ratio(m_timeout_large_us / m_timeout_small_us)
            , m_last_read_position(0)
            , m_last_read_packet(0)
            , m_last_read_frame(0)
            {
                m_log.Trace("ShmIfce()");

                // initalise the clock for m_timeout etc
                if (clock_gettime(CLOCK_REALTIME, &m_time) == -1)
                {
                    m_log.Error("clock_gettime");
                    exit(EXIT_FAILURE);
                }
                m_timeout_ratio = m_timeout_large_us/m_timeout_small_us;
            }

            ~ShmIfce()
            {
                m_log.Trace("~ShmIfce()");
                if(m_file_is_open)
                {
                    closeShm();
                }
            }

            void init()
            {
                if(m_file_is_open)
                {
                    closeShm();
                }

            }

            void openShm(std::string name, IMAGE * shm_p, int node = -1)
            {
                m_log.Trace("openShm(%s, - )", name.c_str());
                if(m_file_is_open)
                {
                    closeShm();
                    m_log.Error("Error: File %s already open. Closing shm and retrying", name.c_str());
                    openShm(name, shm_p);
                }
                
                m_shm = shm_p;
                m_shm_filename = name;
    
                m_shm_file = open(m_shm_filename.c_str(), O_RDWR);
                if(m_shm_file == -1)
                {
                    m_shm->used = 0;
                    m_log.Error("Cannot import shared memory file %s", m_shm_filename.c_str());
                    closeShm();
                    return;
                }
                else
                {
                    m_file_is_open = true;
                    struct stat file_stat;
                    fstat(m_shm_file, &file_stat);
                    m_shm_filesize = file_stat.st_size;

                    m_log.Debug("File %s size: %zd", m_shm_filename.c_str(), m_shm_filesize);
                    m_map = (IMAGE_METADATA*) mmap(0, m_shm_filesize, PROT_READ | PROT_WRITE, MAP_SHARED, m_shm_file, 0);
                    if(node >= 0)
                    {
                        int numa_node = -1;
                        get_mempolicy(&numa_node, NULL, 0, (void*)m_map, MPOL_F_NODE | MPOL_F_ADDR);
                        m_log.Debug("Memory currently on numa node: %d", numa_node);
                        m_log.Debug("Moving memory to node %d", node);
                        numa_tonode_memory((void*) m_map, m_shm_filesize, node);
                        get_mempolicy(&numa_node, NULL, 0, (void*)m_map, MPOL_F_NODE | MPOL_F_ADDR);
                        m_log.Debug("Memory now on numa node: %d", numa_node);
                    }
                    else
                    {
                        m_log.Debug("node: %d", node);
                    }

                    if (m_map == MAP_FAILED) 
                    {
                        closeShm();
                        m_log.Error("Error: mmapping the file");
                        return;
                    }

                    m_shm->memsize = m_shm_filesize;
                    m_shm->shmfd =m_shm_file;
                    m_shm->md = m_map;
                    m_atype = m_shm->md[0].atype;
                    m_shm->md[0].shared = 1;
                    m_shm_nElements = m_shm->md[0].size[0]* m_shm->md[0].size[1];

                    m_log.Debug("image size = %ld x %ld: %ld", (long) m_shm->md[0].size[0], (long) m_shm->md[0].size[1], m_shm_nElements);
                    
                    if(m_shm->md[0].size[0]<1)
                    {
                        m_log.Error("IMAGE \"%s\" AXIS SIZE < 1... ABORTING", m_shm_filename.c_str());
                        closeShm();
                        return;
                    }
                    if(m_shm->md[0].size[1]<1)
                    {
                        m_log.Error("IMAGE \"%s\" AXIS SIZE < 1... ABORTING", m_shm_filename.c_str());
                        closeShm();
                        return;
                    }

                    m_mapv = (char*) m_map;
                    m_mapv += sizeof(IMAGE_METADATA);

                    m_log.Debug("m_atype = %d", (int) m_atype);
                    // hack due to using void pointer.
                    m_shm->array.V = (void *) m_mapv;
                    m_mapv += sizeof(T) * m_shm->md[0].nelement;

                    int kw;
                    m_shm->kw = (IMAGE_KEYWORD*) (m_mapv);
                    for(kw=0; kw<m_shm->md[0].NBkw; kw++)
                    {
                        if(m_shm->kw[kw].type == 'L')
                            m_log.Debug("%d  %s %ld %s", kw, m_shm->kw[kw].name, m_shm->kw[kw].value.numl, m_shm->kw[kw].comment);
                        if(m_shm->kw[kw].type == 'D')
                            m_log.Debug("%d  %s %lf %s", kw, m_shm->kw[kw].name, m_shm->kw[kw].value.numf, m_shm->kw[kw].comment);
                        if(m_shm->kw[kw].type == 'S')
                            m_log.Debug("%d  %s %s %s", kw, m_shm->kw[kw].name, m_shm->kw[kw].value.valstr, m_shm->kw[kw].comment);
                    }

                    m_mapv += sizeof(IMAGE_KEYWORD)*m_shm->md[0].NBkw;
                    strcpy(m_shm->name, m_shm_filename.c_str());
                    // looking for semaphores
                    int sem_number = 0;
                    bool sem_OK = false;
                    sem_t *stest;
                    while(!sem_OK)
                    {
                        std::ostringstream sem_name;
                        sem_name << m_shm->md[0].name << "_sem" << std::setw(2) << std::setfill('0') << sem_number;
                        m_log.Debug("semaphore %s", sem_name.str().c_str());
                        
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
                    
                    m_log.Debug("%d semaphores detected  (m_shm->md[0].sem = %d)", sem_number, (int) m_shm->md[0].sem);
                    m_shm->semptr = (sem_t**) malloc(sizeof(sem_t*) * m_shm->md[0].sem);
                    for(int i = 0; i < sem_number; i++)
                    {
                        std::ostringstream sem_name;
                        sem_name << m_shm->md[0].name << "_sem" << std::setw(2) << std::setfill('0') << i;
                        if ((m_shm->semptr[i] = sem_open(sem_name.str().c_str(), 0, 0644, 0))== SEM_FAILED) 
                        {
                            m_log.Debug("could not open semaphore %s", sem_name.str().c_str());
                        }
                    }
                    
                    std::ostringstream sem_logname;
                    // sprintf(sname, "%s_semlog", image->md[0].name);
                    sem_logname << m_shm->md[0].name << "_semlog";
                    if ((m_shm->semlog = sem_open(sem_logname.str().c_str(), 0, 0644, 0))== SEM_FAILED) 
                    {
                        m_log.Debug("could not open semaphore %s", sem_logname.str().c_str());
                    }         
                }

                m_ptr = reinterpret_cast<T*>(m_shm->array.F);




            }

            void closeShm()
            {
                if(m_file_is_open)
                {
                    // unmap memory
                    m_log.Debug("Unmapping m_shm memory ");
                    munmap(m_map, m_shm_filesize);

                    // close file
                    m_log.Debug("Closing file - %s", m_shm_filename.c_str());
                    close(m_shm_file);

                    // set file is closed set all variables to default.
                    m_file_is_open = false;
                    m_ptr = NULL;
                    reset();
                }
                else
                {
                    m_log.Error("Error: No file open cannot close %s", m_shm_filename.c_str());
                }
            }

            void getFrame()
            {
                if(m_file_is_open)
                {
                    sem_wait(m_shm[0].semptr[1]);
                }
                else
                {
                    m_log.Error("Error: No file open cannot wait for data");
                }
            }

            int getFrame(volatile bool* system_running)
            {
                if(m_file_is_open)
                {
                    size_t i = 0;
                    while(true)
                    {
                        if (clock_gettime(CLOCK_REALTIME, &m_time) == -1) 
                        {
                            m_log.Error("clock_gettime");
                            exit(EXIT_FAILURE);
                        }
                        m_time.tv_nsec +=(int) m_timeout_small_us*1000;
                        if (m_time.tv_nsec>=1000000000)
                        {
                            m_time.tv_sec += m_time.tv_nsec / 1000000000;
                            m_time.tv_nsec %= 1000000000;
                        }
                        int rVal = sem_timedwait(m_shm[0].semptr[1], &m_time);
                        if(rVal == 0)
                        {
                            return 0;
                        }
                        if(!system_running)
                        {
                            m_log.Info("exit requested");
                            return -1;
                        }
                        if (i >= m_timeout_ratio)
                        {
                            // printf("i: %zu, m_timeout_ratio: %zu\n", i, m_timeout_ratio);
                            return -10;
                        }
                        i = i + 1;
                    }
                }
                else
                {
                    m_log.Error("Error: No file open cannot wait for data");
                }
                return -1;
            }

            T * getPtr()
            {
                if(m_file_is_open)
                {
                    return m_ptr; //reinterpret_cast<T*>(m_shm->array.F);
                }

                m_log.Error("Error: No file open cannot close %s", m_shm_filename.c_str());
                return NULL;
            }

            // These are full frame writes
            void writeData(T * data)
            {
                int semval = 0;
                m_shm[0].md[0].write = 1;
                T* dst = getPtr();

                // copy data in.
                memcpy(dst,data, m_shm_nElements*sizeof(T));

                for(int i = 0; i < m_shm[0].md[0].sem; i++)
                {
                    sem_getvalue(m_shm[0].semptr[i], &semval);
                    if(semval < SEMAPHORE_MAXVAL )
                        sem_post(m_shm[0].semptr[i]);
                }

                if(m_shm[0].semlog != NULL)
                {
                    sem_getvalue(m_shm[0].semlog, &semval);
                    if(semval < SEMAPHORE_MAXVAL)
                    {
                        sem_post(m_shm[0].semlog);
                    }
                }

                m_shm[0].md[0].write = 0;
                m_shm[0].md[0].cnt0++;
            }

            void writeData()
            {
                int semval = 0;
                m_shm[0].md[0].write = 1;

                for(int i = 0; i < m_shm[0].md[0].sem; i++)
                {
                    sem_getvalue(m_shm[0].semptr[i], &semval);
                    if(semval < SEMAPHORE_MAXVAL )
                        sem_post(m_shm[0].semptr[i]);
                }

                if(m_shm[0].semlog != NULL)
                {
                    sem_getvalue(m_shm[0].semlog, &semval);
                    if(semval < SEMAPHORE_MAXVAL)
                    {
                        sem_post(m_shm[0].semlog);
                    }
                }

                m_shm[0].md[0].write = 0;
                m_shm[0].md[0].cnt0++;
            }

            void writeData(int32_t counter)
            {
                int semval = 0;
                m_shm[0].md[0].write = 1;
                m_shm[0].md[0].cnt2 = counter;

                for(int i = 0; i < m_shm[0].md[0].sem; i++)
                {
                    sem_getvalue(m_shm[0].semptr[i], &semval);
                    if(semval < SEMAPHORE_MAXVAL )
                        sem_post(m_shm[0].semptr[i]);
                }

                if(m_shm[0].semlog != NULL)
                {
                    sem_getvalue(m_shm[0].semlog, &semval);
                    if(semval < SEMAPHORE_MAXVAL)
                    {
                        sem_post(m_shm[0].semlog);
                    }
                }

                m_shm[0].md[0].write = 0;
                m_shm[0].md[0].cnt0++;

                struct timespec t;
                clock_gettime(CLOCK_REALTIME, &t);
                m_shm[0].md[0].atime.tsfixed.secondlong = (unsigned long)(1e9 * t.tv_sec + t.tv_nsec);
            }

            void initialConnectToStream()
            {
                if(m_file_is_open)
                {
                    sem_wait(m_shm[0].semptr[1]);
                    // get information
                    m_last_read_frame = m_shm[0].md[0].cnt0;
                    m_last_read_position = 0;
                    m_last_read_packet = 0;
                }
                else
                {
                    m_log.Error("Error: No file open cannot wait for data");
                }   
            }

            // write Packets
            void writeStartPacketData()
            {

            }

            void writePacketData()
            {

            }

            void writeFinalisePacketData()
            {

            }

            inline int32_t getCounter()
            {
                return m_shm[0].md[0].cnt2;
            }
        
            inline size_t size(){return m_shm_nElements;}

            /*
            // wait on the packet
            T * waitOnPacket(size_t &packetReceived, size_t &length)
            {
                // spin on expected position
                while(m_last_read_frame >= () )
                {
                    // return pointer to region of m_shm.
                    size_t start = m_shm[0].md[0].__;
                    packetReceived = m_shm[0].md[0].cnt0;
                    length = m_shm[0].md[0].__;
                    return m_ptr[start];
                }
            }
            */

            void releaseAllSemaphores()
            {
                int semval = 0;
                for(int i = 0; i < m_shm[0].md[0].sem; i++)
                {
                    sem_getvalue(m_shm[0].semptr[i], &semval);
                    if(semval < SEMAPHORE_MAXVAL )
                        sem_post(m_shm[0].semptr[i]);
                }

                if(m_shm[0].semlog != NULL)
                {
                    sem_getvalue(m_shm[0].semlog, &semval);
                    if(semval < SEMAPHORE_MAXVAL)
                    {
                        sem_post(m_shm[0].semlog);
                    }
                }
            }

            void clean_sems()
            {
                m_log.Trace("clean_sems()");
                m_log.Debug("Cleaning semaphores to begin processing");
                int retval = 0;
                for(int i = 0; i < m_shm[0].md[0].sem; i++)
                {
                    m_log.Debug("cleaning semaphore %d", i);
                    while(retval>=0)
                    {
                        retval = sem_trywait(m_shm[0].semptr[i]);
                    }
                    m_log.Debug("rSemaphore %d clean", i);
                    retval = 0;
                }
            }

        protected:

            void reset()
            {
                if(m_file_is_open)
                {
                    closeShm();
                }

                m_map = 0;
                m_ptr = 0;
                m_shm_filename = "";
                m_shm_filesize = 0;
                m_shm_nElements = 0;
                m_mapv = 0;
                m_atype = 0;
                m_file_is_open = false;
                m_timeout_small_us = 10000;
                m_timeout_large_us = 10000000;
                m_timeout_ratio = m_timeout_large_us / m_timeout_small_us;

                if (clock_gettime(CLOCK_REALTIME, &m_time) == -1)
                {
                    perror("clock_gettime");
                    exit(EXIT_FAILURE);
                }
            }

            IMAGE * m_shm;
            IMAGE_METADATA *m_map;
            T * m_ptr;
            Log::Logger& m_log;

            std::string m_shm_filename;

            int m_shm_file;

            size_t m_shm_filesize;
            size_t m_shm_nElements;

            char *m_mapv;
            uint8_t m_atype;
            bool m_file_is_open;

            // timeout stuff
            struct timespec m_time;
            size_t m_timeout_small_us;
            size_t m_timeout_large_us;
            size_t m_timeout_ratio;

            // for packeting
            size_t m_last_read_position;
            size_t m_last_read_packet;
            size_t m_last_read_frame;
    };
}
#endif