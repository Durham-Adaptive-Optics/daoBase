#ifndef DAO_SHM_IFCE_HPP
#define DAO_SHM_IFCE_HPP

#include "dao.h"
#include <daoLog.hpp>
#include <chrono>
#include <ctime>
#include <unistd.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#ifndef __APPLE__
    #include <numa.h>
    #include <numaif.h>
#endif

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iomanip>
#include <type_traits>
#include <time.h>
#include <atomic>

namespace Dao
{
    template<class T>
    class ShmIfce {
        public:
            // Constructor that matches Python interface - can create new or connect to existing
            ShmIfce(Log::Logger& logger, const std::string& fname, T* data = nullptr, size_t dataSize = 0) 
                : m_log(logger)
                , m_shm_filename(fname)
                , m_file_is_open(true)
            {
                m_log.Trace("ShmIfce2()");
                m_image = new IMAGE();

                if (data != nullptr) {
                    // Create new shared memory
                    m_log.Info("%s will be created or overwritten", fname.c_str());
                    uint32_t size[2] = {(uint32_t)dataSize, 1};
                    daoShmImageCreate(m_image, fname.c_str(), 2, size, getDataType(), 1, 0);
                    set_data(data, dataSize);
                } else {
                    // Connect to existing
                    m_log.Info("Loading existing %s", fname.c_str());
                    daoShmShm2Img(fname.c_str(), m_image);
                }

                initializeMembers();
            }

            ~ShmIfce() {
                close();
            }

            // Match Python interface methods
            void set_data() {
                m_log.Trace("set_data() - finalizing direct memory write");
                if (!m_file_is_open) {
                    m_log.Error("No file open");
                    return;
                }
                // Finalize the write using dao.h function
                daoShmImagePart2ShmFinalize(m_image);
            }

            void set_data(T* data, size_t dataSize) {
                m_log.Trace("set_data()");
                if (!m_file_is_open) {
                    m_log.Error("No file open");
                    return;
                }
                daoShmImage2Shm(data, dataSize, m_image);
            }

            void get_data(T* buffer) {
                m_log.Trace("get_data(buffer)");
                if (!m_file_is_open) {
                    m_log.Error("No file open");
                    return;
                }
                // Copy data to provided buffer
                memcpy(buffer, GetPtr(), m_shm_nElements * sizeof(T));
            }

            T* get_data(bool check = false, int semaphore = 1, double timeout = 0.0) {
                m_log.Trace("get_data()");
                if (!m_file_is_open) {
                    m_log.Error("No file open");
                    return nullptr;
                }

                if (check) {
                    if (timeout > 0.0) {
                        struct timespec ts;
                        clock_gettime(CLOCK_REALTIME, &ts);
                        ts.tv_sec += static_cast<time_t>(timeout);
                        ts.tv_nsec += static_cast<long>((timeout - static_cast<time_t>(timeout)) * 1e9);
                        if (ts.tv_nsec >= 1000000000) {
                            ts.tv_sec += 1;
                            ts.tv_nsec -= 1000000000;
                        }
                        if (daoShmWaitForSemaphoreTimeout(m_image, semaphore, ts) != 0) {
                            m_log.Error("Timeout waiting for semaphore");
                            return nullptr;
                        }
                    } else {
                        daoShmWaitForSemaphore(m_image, semaphore);
                    }
                }
                
                return GetPtr();
            }

            T* get_data_spin() {
                m_log.Trace("get_data_spin()");
                if (!m_file_is_open) {
                    m_log.Error("No file open");
                    return nullptr;
                }

                uint64_t counter = m_image->md->cnt0;
                while (m_image->md->cnt0 <= counter) {
                    #if defined(__x86_64__) || defined(_M_X64)
                        __asm__ __volatile__ ("pause" ::: "memory");
                    #elif defined(__aarch64__)
                        __asm__ __volatile__("yield" ::: "memory");
                    #else
                        // Minimal busy-wait on other architectures
                        std::atomic_thread_fence(std::memory_order_seq_cst);
                    #endif
                }

                if (m_image->md->cnt0 != counter + 1) {
                    m_log.Warning("Frame discontinuity detected");
                }

                return GetPtr();
            }

            T* get_data_block(size_t nFrames, int semaphore = 0, double timeout = 0.0) {
                m_log.Trace("get_data_block()");
                if (!m_file_is_open) {
                    m_log.Error("No file open");
                    return nullptr;
                }

                struct timespec ts;
                struct timespec* ts_ptr = nullptr;
                if (timeout > 0.0) {
                    clock_gettime(CLOCK_REALTIME, &ts);
                    ts.tv_sec += static_cast<time_t>(timeout);
                    ts.tv_nsec += static_cast<long>((timeout - static_cast<time_t>(timeout)) * 1e9);
                    if (ts.tv_nsec >= 1000000000) {
                        ts.tv_sec += 1;
                        ts.tv_nsec -= 1000000000;
                    }
                    ts_ptr = &ts;
                }

                // Pre-allocate buffer for the block
                size_t bufferSize = nFrames * m_shm_nElements;
                T* buffer = new T[bufferSize];

                int result = daoShmGetDataBlock(m_image, buffer, nFrames, semaphore, ts_ptr);
                if (result != 0) {
                    delete[] buffer;
                    return nullptr;
                }

                return buffer;
            }

            uint64_t get_counter() const {
                return daoShmGetCounter(m_image);
            }

            uint64_t get_frame_id() const {
                return m_image->md->cnt2;
            }

            std::chrono::system_clock::time_point get_timestamp() const {
                uint64_t nanoseconds = m_image->md->atime.tsfixed.secondlong;
                std::chrono::nanoseconds ns(nanoseconds);
                return std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(ns));
            }

            void close() {
                m_log.Trace("close()");
                if (m_file_is_open) {
                    daoShmCloseShm(m_image);
                    delete m_image;
                    m_file_is_open = false;
                }
            }

            T* GetPtr() const { return m_ptr; }

            // Add new spin wait methods
            bool get_data_spin(uint64_t counter) {
                m_log.Trace("wait_for_counter()");
                if (!m_file_is_open) {
                    m_log.Error("No file open");
                    return false;
                }

                // Get current counter and spin until it changes
                uint64_t current = m_image->md->cnt0;
                while (current == counter) {
                    // Short sleep to prevent CPU overload
                    usleep(1);
                    current = m_image->md->cnt0;
                }

                if (current != counter + 1) {
                    m_log.Warning("Frame discontinuity detected");
                }
                
                return true;
            }

        protected:
            // Helper methods
            void initializeMembers() {
                m_file_is_open = true;
                m_shm = m_image;
                m_map = m_image->md;
                m_atype = m_image->md->atype;
                m_shm_nElements = m_image->md->size[0] * m_image->md->size[1];
                m_ptr = reinterpret_cast<T*>(m_image->array.V);
            }

            uint8_t getDataType() {
                if constexpr(std::is_same_v<T, uint8_t>) return 1;
                else if constexpr(std::is_same_v<T, int8_t>) return 2;
                else if constexpr(std::is_same_v<T, uint16_t>) return 3;
                else if constexpr(std::is_same_v<T, int16_t>) return 4;
                else if constexpr(std::is_same_v<T, uint32_t>) return 5;
                else if constexpr(std::is_same_v<T, int32_t>) return 6;
                else if constexpr(std::is_same_v<T, uint64_t>) return 7;
                else if constexpr(std::is_same_v<T, int64_t>) return 8;
                else if constexpr(std::is_same_v<T, float>) return 9;
                else if constexpr(std::is_same_v<T, double>) return 10;
                else return 0;
            }

            IMAGE* m_image;
            IMAGE* m_shm;
            IMAGE_METADATA* m_map;
            T* m_ptr;
            Log::Logger& m_log;
            std::string m_shm_filename;
            bool m_file_is_open;
            size_t m_shm_nElements;
            uint8_t m_atype;
    };
}
#endif