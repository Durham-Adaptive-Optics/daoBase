/**
 * @ Author: Thomas N. Davies
 * @ Company: Centre for Advanced Instrumentation, Durham University
 * @ Contact: thomas.n.davies@durham.ac.uk
 * @ Create Time: 2025-10-09 16:23:09
 * @ Description: C++ DAO Shared Memory Interface
 */

#ifndef DAO_SHM_HPP
#define DAO_SHM_HPP

#include <stdexcept>
#include <string>
#include <time.h>
#include <dao.h>

namespace Dao 
{
    template <typename T>
    class shm {
        public:
        /**
         * @brief Syncronization options when loading shared memory frame.
         */
        enum class sync : int32_t {
            SEM0 = 0, 
            SEM1, SEM2, SEM3,
            SEM4, SEM5, SEM6,
            SEM7, SEM8, SEM9, 
            SEM, SPIN, NONE
        };

        /**
         * @brief Create and open a Dao shared memory.
         * @param name Shared memory name.
         * @param initialData Initial shared memory data frame.
         * @param nAxes Number of axes in the shared memory data frame.
         * @param shape Number of elements in each axis.
         */
        shm(const std::string &name, T *initialData, long nAxes, uint32_t *shape) {
            const auto status = daoShmImageCreate(
                &image_,
                name.c_str(),
                nAxes,
                shape,
                inferDaoType(),
                1, // shared memory
                0 // no keywords
            );
            
            if(status != DAO_SUCCESS)
                throw std::runtime_error("failed to create dao shared memory");

            if(initialData)
                set_data(initialData);
        }

        /**
         * @brief Open a pre-existing Dao shared memory.
         * @param name Shared memory name.
         */
        shm(const std::string &name) {
            const auto status = daoShmShm2Img(name.c_str(), &image_);

            if(status != DAO_SUCCESS)
                throw std::runtime_error("failed to open dao shared memory");
        }

        /**
         * @brief Releases resources used in accessing the shared memory object,
         * however the object itself will still persist afterward.
         */
        ~shm() {
            daoShmCloseShm(&image_);
        }

        /**
         * @brief Writes new frame array into the shared memory.
         * @param frameData Pointer to the frame array.
         */
        void set_data(const T *frameArray) {
            daoShmImage2Shm(frameArray, image_.md->nelement, &image_);
        }

        /**
         * @brief Retrieve a pointer to the shared memory frame array.
         * Optionally blocks until the next frame is written to shared memory.
         * @param sync Synchronization option (see Dao::Shm::Sync for different options).
         * @param semTimeout Timeout for when a semaphore sync option is used (in seconds).
         * @return Pointer to the shared memory frame array, or nullptr if synchronization
         * failed internally.
         */
        T* get_data(sync sync = sync::NONE, size_t semTimeout = 0) const {
            switch(sync) {
                case sync::NONE: {} break;

                case sync::SPIN: {
                    if(daoShmWaitForCounter(&image_) != DAO_SUCCESS)
                        return nullptr;
                } break;

                default: {
                    timespec ts;
                    clock_gettime(CLOCK_REALTIME, &ts);
                    ts.tv_sec += semTimeout;

                    const int32_t semNb = (sync == sync::SEM) ? (int32_t)sync::SEM0 : (int32_t)sync;
                    const auto status = semTimeout ? daoShmWaitForSemaphoreTimeout(&image_, semNb, &ts) :  
                        daoShmWaitForSemaphore(&image_, semNb);

                    if(status != DAO_SUCCESS)
                        return nullptr;
                } break;
            }

            return (T*)image_.array.V;
        }

        /**
         * @brief Get current metadata cnt0 value.
         * @return cnt0.
         */
        uint64_t get_counter() const {
            volatile IMAGE_METADATA *md = (volatile IMAGE_METADATA*)image_.md;
            return md->cnt0;
        }

        /**
         * @brief Get current cnt1 value.
         * @return cnt1.
         */
        uint64_t get_cnt1() const {
            volatile IMAGE_METADATA *md = (volatile IMAGE_METADATA*)image_.md;
            return md->cnt1;
        }

        /**
         * @brief Get current frame id (cnt2).
         * @return frame id (cnt2).
         */
        uint64_t get_frame_id() const {
            volatile IMAGE_METADATA *md = (volatile IMAGE_METADATA*)image_.md;
            return md->cnt2;
        }

        /**
         * @brief Get current timestamp value.
         * @return Seconds since Unix epoch.
         */
        int64_t get_timestamp() const {
            volatile IMAGE_METADATA *md = (volatile IMAGE_METADATA*)image_.md;
            return md->atime.tsfixed.secondlong;
        }

        /**
         * @brief Get shared memory metadata.
         * @return Poiter to shared memory metadata structure.
         */
        const IMAGE_METADATA* get_meta_data() const { return image_.md; }

        private:
        /**
         * @brief Compile time inference of dtype from T.
         * @return Dao data type (dtype).
         */
        auto inferDaoType() const {
            if constexpr(std::is_same_v<T, uint8_t>) return _DATATYPE_UINT8;
            else if(std::is_same_v<T, int8_t>) return _DATATYPE_INT8;
            else if(std::is_same_v<T, uint16_t>) return _DATATYPE_UINT16;
            else if(std::is_same_v<T, int16_t>) return _DATATYPE_INT16;
            else if(std::is_same_v<T, uint32_t>) return _DATATYPE_UINT32;
            else if(std::is_same_v<T, int32_t>) return _DATATYPE_INT32;
            else if(std::is_same_v<T, uint64_t>) return _DATATYPE_UINT64;
            else if(std::is_same_v<T, int64_t>) return _DATATYPE_INT64;
            else if(std::is_same_v<T, float>) return _DATATYPE_FLOAT;
            else if(std::is_same_v<T, double>) return _DATATYPE_DOUBLE;
            else if(std::is_same_v<T, complex_float>) return _DATATYPE_COMPLEX_FLOAT;
            else if(std::is_same_v<T, complex_double>) return _DATATYPE_COMPLEX_DOUBLE;
        }

        IMAGE image_;
    };
};
#endif // DAO_SHM_HPP
