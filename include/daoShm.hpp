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
#include <vector>
#include <dao.h>

namespace Dao
{
    using Shape = std::vector<uint32_t>; 

    /**
     * @brief Syncronization options for shared memory frames.
     */
    enum class ShmSync : int32_t {
        SEM0 = 0, 
        SEM1, SEM2, SEM3,
        SEM4, SEM5, SEM6,
        SEM7, SEM8, SEM9, 
        SEM, SPIN, NONE
    };

    template <typename T>
    class Shm {
        public:
        /**
         * @brief Create and open a Dao shared memory.
         * @param name Shared memory name.
         * @param shape Dao::Shape containing number of elements in each axis.
         * @param frame Initial shared memory data frame.
         */
        Shm(const std::string &name, const Dao::Shape &shape, T *frame = nullptr,
            uint32_t depth = 1) {
            if(shape.size() != 2 && shape.size() != 3)
                throw std::runtime_error("invalid dao shape");

            const auto status = daoShmImageCreate_FIFO(
                &image_,
                name.c_str(),
                shape.size(),
                (uint32_t*)shape.data(),
                inferDaoType(),
                1, // shared memory
                0, // no keywords
                depth
            );
            md_ = (volatile IMAGE_METADATA *)image_.md;
            
            if(status != DAO_SUCCESS)
                throw std::runtime_error("failed to create dao shared memory");

            if(frame)
                set_frame(frame);
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
         * @param frame Pointer to the frame array.
         */
        void set_frame(const T *frame) {
            daoShmImage2Shm((T*)frame, image_.md->nelement, &image_);
        }

        /**
         * @brief Retrieve a pointer to the shared memory frame array.
         * Optionally blocks until the next frame is written to shared memory.
         * @param sync Synchronization option (see Dao::ShmSync).
         * @return Pointer to the shared memory frame array, or nullptr if synchronization
         * failed internally.
         */
        T* get_frame(ShmSync sync = ShmSync::NONE) {
            switch(sync) {
                case ShmSync::NONE: {} break;

                case ShmSync::SPIN: {
                    if(daoShmWaitForCounter(&image_) != DAO_SUCCESS)
                        return nullptr;
                } break;

                default: {
                    const int32_t semNb = (sync == ShmSync::SEM) ? (int32_t)ShmSync::SEM0 : (int32_t)sync;
                    if(daoShmWaitForSemaphore(&image_, semNb) != DAO_SUCCESS)
                        return nullptr;
                } break;
            }
            return (T*)image_.array.V;
        }

        /**
         * @brief Retrieve a pointer to the newest segment of the shared memory frame array.
         * Optionally blocks until the next frame is written to shared memory.
         * @param sync Synchronization option (see Dao::ShmSync).
         * @param syncValue Specifies the timeout (seconds) for the semaphore sync options, or the cnt0 value to sync on when using the SPIN sync option.
         * @return Pointer to the shared memory frame array, or nullptr if synchronization
         * failed internally.
         */
        T* get_frame(ShmSync sync, size_t syncValue) {
            switch(sync) {
                case ShmSync::NONE: {} break;

                case ShmSync::SPIN: {
                    if(daoShmWaitForTargetCounter(&image_, syncValue) != DAO_SUCCESS)
                        return nullptr;
                } break;

                default: {
                    timespec ts;
                    clock_gettime(CLOCK_REALTIME, &ts);
                    ts.tv_sec += syncValue;

                    const int32_t semNb = (sync == ShmSync::SEM) ? (int32_t)ShmSync::SEM0 : (int32_t)sync;
                    if(daoShmWaitForSemaphoreTimeout(&image_, semNb, &ts) != DAO_SUCCESS)
                        return nullptr;
                } break;
            }

            void *newest_data;
            uint32_t segment_idx;
            uint64_t segment_cnt0;
            daoShmGetNewestSegment(&image_, &newest_data, &segment_idx, &segment_cnt0);

            return (T*)newest_data;
        }

        /**
         * @brief Retrieve a pointer to the next segment of the shared memory frame array.
         * Optionally blocks until the next frame is written to shared memory.
         * @param bool Flag to enable spinning until new frame is ready
         * @param status Reference to variable for holding status information
         * @return Pointer to the shared memory frame array, or nullptr if synchronization failed (wait enabled),
         * or if frame is not yet ready (wait disabled)
         */
        T* get_next_frame(bool wait, int_fast8_t &status) {
            // Wait for the next frame if requested
            if (wait) {
                if (daoShmWaitForNextSegment(&image_) != DAO_SUCCESS) {
                    status = DAO_ERROR;
                    return nullptr;
                }
            }

            // Get the next frame
            void *segment_ptr;
            uint32_t segment_idx;
            uint64_t segment_cnt0;

            status = daoShmGetNextSegment(&image_, &segment_ptr, &segment_idx, &segment_cnt0);

            return (T*)segment_ptr;
        }

        /**
         * @brief Retrieve a pointer to the next segment of the shared memory frame array.
         * Optionally blocks until the next frame is written to shared memory.
         * @param bool Flag to enable spinning until new frame is ready
         * @param status Reference to variable for holding status information
         * @param segment_cnt0 CNT0 value of the segment at the time of reading
         * @return Pointer to the shared memory frame array, or nullptr if either synchronization
         * failed (wait enabled), or if frame is not yet ready (wait disabled)
         */
        T* get_next_frame(bool wait, int_fast8_t &status, uint64_t &segment_cnt0) {
            // Wait for the next frame if requested
            if (wait) {
                if (daoShmWaitForNextSegment(&image_) != DAO_SUCCESS) {
                    status = DAO_ERROR;
                    return nullptr;
                }
            }

            // Get the next frame
            void *segment_ptr;
            uint32_t segment_idx;

            status = daoShmGetNextSegment(&image_, &segment_ptr, &segment_idx, &segment_cnt0);

            return (T*)segment_ptr;
        }

        /**
         * @brief Check if the last frame read from the FIFO has been overwritten.
         * @return Status code, either DAO_SUCCESS if not overwritten, or DAO_OVERWRITE otherwise.
         */
        int_fast8_t check_segment_overwrite() {
            return daoShmCheckSegmentOverwrite(&image_);
        }

        /**
         * @brief Retrieve a pointer to a given segment of the shared memory frame array.
         * @param bool Flag to enable spinning until new frame is ready
         * @param overwrite Reference to flag indicating frame overwrite (optional)
         * @return Pointer to the shared memory frame array, or nullptr if synchronization failed (wait enabled),
         * or if frame is not yet ready (wait disabled)
         */
        T* get_arbitrary_frame(uint32_t segment_idx) {
            // Get the next frame
            void *segment_ptr;
            uint64_t *segment_cnt0;

            daoShmGetArbitrarySegment(&image_, &segment_ptr, segment_idx);

            return segment_ptr;
        }

        /**
         * @brief Get shape.
         * @return Dao::Shape.
         */
        Dao::Shape get_shape() const {
            return Dao::Shape(image_.md->size, image_.md->size + image_.md->naxis);            
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
            return this->get_counter(md_->fifo_last_written);
        }

        /**
         * @brief Get arbitrary metadata cnt0 value.
         * @return cnt0.
         */
        uint64_t get_counter(uint32_t fifo_idx) const {
            volatile IMAGE_METADATA *segment_md_ = &(md_[fifo_idx % (md_->fifo_size)]);

            return segment_md_->cnt0;
        }

        /**
         * @brief Get current cnt1 value.
         * @return cnt1.
         */
        uint64_t get_cnt1() const {
            return this->get_cnt1(md_->fifo_last_written);
        }

        /**
         * @brief Get arbitrary cnt1 value.
         * @return cnt1.
         */
        uint64_t get_cnt1(uint32_t fifo_idx) const {
            volatile IMAGE_METADATA *segment_md_ = &(md_[fifo_idx % (md_->fifo_size)]);

            return segment_md_->cnt1;
        }

        /**
         * @brief Get current frame id (cnt2).
         * @return frame id (cnt2).
         */
        uint64_t get_frame_id() const {
            return this->get_frame_id(md_->fifo_last_written);
        }

        /**
         * @brief Get arbitrary frame id (cnt2).
         * @return frame id (cnt2).
         */
        uint64_t get_frame_id(uint32_t fifo_idx) const {
            volatile IMAGE_METADATA *segment_md_ = &(md_[fifo_idx % (md_->fifo_size)]);

            return segment_md_->cnt2;
        }

        /**
         * @brief Get current timestamp value.
         * @return Seconds since Unix epoch.
         */
        int64_t get_timestamp() const {
            return this->get_timestamp(md_->fifo_last_written);
        }

        /**
         * @brief Get arbitrary timestamp value.
         * @return Seconds since Unix epoch.
         */
        int64_t get_timestamp(uint32_t fifo_idx) const {
            volatile IMAGE_METADATA *segment_md_ = &(md_[fifo_idx % (md_->fifo_size)]);

            return segment_md_->atime.tsfixed.secondlong;
        }

        /**
         * @brief Get shared memory metadata.
         * @return Pointer to shared memory metadata structure.
         */
        IMAGE_METADATA* get_meta_data() const {
            return this->get_meta_data(md_->fifo_last_written);
        }

        /**
         * @brief Get shared memory metadata.
         * @return Pointer to shared memory metadata structure.
         */
        IMAGE_METADATA* get_meta_data(uint32_t fifo_idx) const {
            volatile IMAGE_METADATA *segment_md_ = &(md_[fifo_idx % (md_->fifo_size)]);

            return const_cast<IMAGE_METADATA*>(segment_md_);
        }

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
       IMAGE image_ {};
       volatile IMAGE_METADATA *md_;

    };
};
#endif // DAO_SHM_HPP
