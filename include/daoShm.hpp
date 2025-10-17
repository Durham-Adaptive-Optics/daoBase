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
    class Shm {
        public:
        /**
         * @brief Syncronization options when loading shared memory frame.
         */
        enum class Sync : int32_t {
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
        Shm(const std::string &name, T *initialData, long nAxes, uint32_t *shape) {
            const auto status = daoShmImageCreate(
                &image_,
                name.c_str(),
                nAxes,
                shape,
                inferDaoType(),
                1, // shared memory
                0 // no keywords
            );
            md_ = (volatile IMAGE_METADATA *)image_.md;
            
            if(status != DAO_SUCCESS)
                throw std::runtime_error("failed to create dao shared memory");

            if(initialData)
                set_data(initialData);
        }

        /**
         * @brief Open a pre-existing Dao shared memory.
         * @param name Shared memory name.
         */
        Shm(const std::string &name) {
            const auto status = daoShmShm2Img(name.c_str(), &image_);
            md_ = (volatile IMAGE_METADATA *)image_.md;

            if(status != DAO_SUCCESS)
                throw std::runtime_error("failed to open dao shared memory");
        }

        /**
         * @brief Releases resources used in accessing the shared memory object,
         * however the object itself will still persist afterward.
         */
        ~Shm() {
            daoShmCloseShm(&image_);
        }

        /**
         * @brief Writes new frame array into the shared memory.
         * @param frameData Pointer to the frame array.
         */
        void set_data(const T *frameArray) {
            daoShmImage2Shm((T*)frameArray, image_.md->nelement, &image_);
        }

        /**
         * @brief Retrieve a pointer to the shared memory frame array.
         * Optionally blocks until the next frame is written to shared memory.
         * @param sync Synchronization option (see Dao::Shm::Sync).
         * @return Pointer to the shared memory frame array, or nullptr if synchronization
         * failed internally.
         */
        T* get_data(Sync sync = Sync::NONE) {
            switch(sync) {
                case Sync::NONE: {} break;

                case Sync::SPIN: {
                    const auto status = daoShmWaitForCounter(&image_);
                    if(status != DAO_SUCCESS)
                        return nullptr;
                } break;

                default: {
                    const int32_t semNb = (sync == Sync::SEM) ? (int32_t)Sync::SEM0 : (int32_t)sync;
                    const auto status = daoShmWaitForSemaphore(&image_, semNb);
                    if(status != DAO_SUCCESS)
                        return nullptr;
                } break;
            }
            return (T*)image_.array.V;
        }

        /**
         * @brief Retrieve a pointer to the shared memory frame array.
         * Optionally blocks until the next frame is written to shared memory.
         * @param sync Synchronization option (see Dao::Shm::Sync).
         * @param syncValue Specifies the timeout (seconds) for the semaphore sync options, or the cnt0 value to sync on when using the SPIN sync option.
         * @return Pointer to the shared memory frame array, or nullptr if synchronization
         * failed internally.
         */
        T* get_data(Sync sync, size_t syncValue) {
            switch(sync) {
                case Sync::NONE: {} break;

                case Sync::SPIN: {
                    const auto status = daoShmWaitForTargetCounter(&image_, syncValue);
                    if(status != DAO_SUCCESS)
                        return nullptr;
                } break;

                default: {
                    timespec ts;
                    clock_gettime(CLOCK_REALTIME, &ts);
                    ts.tv_sec += syncValue;

                    const int32_t semNb = (sync == Sync::SEM) ? (int32_t)Sync::SEM0 : (int32_t)sync;
                    const auto status = daoShmWaitForSemaphoreTimeout(&image_, semNb, &ts); 
                    if(status != DAO_SUCCESS)
                        return nullptr;
                } break;
            }
            return (T*)image_.array.V;
        }

        /**
         * @brief Get element count.
         * @return nelements.
         */
        uint64_t get_element_count() const {
            return image_.md->nelement;
        }

        /**
         * @brief Get current metadata cnt0 value.
         * @return cnt0.
         */
        uint64_t get_counter() const {
            return md_->cnt0;
        }

        /**
         * @brief Get current cnt1 value.
         * @return cnt1.
         */
        uint64_t get_cnt1() const {
            return md_->cnt1;
        }

        /**
         * @brief Get current frame id (cnt2).
         * @return frame id (cnt2).
         */
        uint64_t get_frame_id() const {
            return md_->cnt2;
        }

        /**
         * @brief Get current timestamp value.
         * @return Seconds since Unix epoch.
         */
        int64_t get_timestamp() const {
            return md_->atime.tsfixed.secondlong;
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

        /*
            === MEMBER VARIABLES ===
        */
       IMAGE image_;
       volatile IMAGE_METADATA *md_;

    };
};
#endif // DAO_SHM_HPP
