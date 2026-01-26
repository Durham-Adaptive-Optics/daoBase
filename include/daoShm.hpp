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
        Shm(const std::string &name, const Dao::Shape &shape, T *frame = nullptr) : is_writer_(true) {
            if(shape.size() != 2 && shape.size() != 3)
                throw std::runtime_error("invalid dao shape");

            const auto status = daoShmImageCreate(
                &image_,
                name.c_str(),
                shape.size(),
                (uint32_t*)shape.data(),
                inferDaoType(),
                1, // shared memory
                0 // no keywords
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
        Shm(const std::string &name) : is_writer_(false) {
            const auto status = daoShmShm2Img(name.c_str(), &image_);
            md_ = (volatile IMAGE_METADATA *)image_.md;

            if(status != DAO_SUCCESS)
                throw std::runtime_error("failed to open dao shared memory");
            
            // Auto-register as reader for backward compatibility
            try {
                register_reader("cpp_reader");
            } catch(...) {
                // Silently ignore registration failures for backward compatibility
            }
        }

        /**
         * @brief Releases resources used in accessing the shared memory object,
         * however the object itself will still persist afterward.
         */
        ~Shm() {
            // Auto-unregister if we're a reader
            if (!is_writer_) {
                try {
                    unregister_reader();
                } catch(...) {
                    // Silently ignore errors during cleanup
                }
            }
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
         * @brief Retrieve a pointer to the shared memory frame array.
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
            return (T*)image_.array.V;
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
        IMAGE_METADATA* get_meta_data() const { return image_.md; }

        /**
         * @brief Register as a reader for this shared memory buffer.
         * 
         * Allocates a dedicated semaphore for this reader process, preventing
         * race conditions when multiple readers access the same buffer.
         * 
         * @param process_name Custom name for this reader process (optional, null for auto-detection).
         * @param preferred_sem Preferred semaphore index (0-9), or -1 for automatic allocation.
         * @return The allocated semaphore index (0-9).
         * @throws std::runtime_error if registration fails.
         */
        int register_reader(const char* process_name = nullptr, int preferred_sem = -1) {
            int sem_index = daoShmRegisterReader(&image_, process_name, preferred_sem);
            
            if(sem_index < 0) {
                if(sem_index == -3) // DAO_NO_SEMAPHORE_AVAILABLE
                    throw std::runtime_error("No semaphores available - all 10 slots are in use");
                else if(sem_index == -4) // DAO_REGISTRATION_STOLEN
                    throw std::runtime_error("Registration was stolen by another process");
                else
                    throw std::runtime_error("Failed to register reader: error code " + std::to_string(sem_index));
            }
            
            return sem_index;
        }

        /**
         * @brief Unregister this reader and release its semaphore.
         * 
         * Should be called when the reader no longer needs to access the shared memory,
         * allowing other readers to use the semaphore slot.
         * 
         * @return 0 on success, negative error code on failure.
         */
        int unregister_reader() {
            return daoShmUnregisterReader(&image_);
        }

        /**
         * @brief Validate that this reader's registration is still valid.
         * 
         * Checks that our process still owns the semaphore slot we registered for.
         * This is a quick check that doesn't involve acquiring locks.
         * 
         * @return true if registration is valid, false otherwise.
         */
        bool validate_registration() {
            return daoShmValidateReaderRegistration(&image_) == 0; // 0 = success/valid
        }

        /**
         * @brief Get information about all currently registered readers.
         * 
         * @return Vector of SEM_REGISTRY_ENTRY structures for all registered readers.
         */
        std::vector<SEM_REGISTRY_ENTRY> get_registered_readers() {
            SEM_REGISTRY_ENTRY registry[IMAGE_NB_SEMAPHORE];
            
            int result = daoShmGetRegisteredReaders(&image_, registry);
            
            if(result < 0)
                return std::vector<SEM_REGISTRY_ENTRY>();
                
            std::vector<SEM_REGISTRY_ENTRY> readers;
            for(int i = 0; i < IMAGE_NB_SEMAPHORE; i++) {
                if(registry[i].locked) {
                    readers.push_back(registry[i]);
                }
            }
            
            return readers;
        }

        /**
         * @brief Force unlock a semaphore slot (administrative function).
         * 
         * WARNING: This should only be used for administrative/debugging purposes.
         * Forcefully unlocking a semaphore that is actively in use can cause race conditions.
         * 
         * @param semNb Semaphore index (0-9) to force unlock.
         * @return 0 on success, negative error code on failure.
         * @throws std::invalid_argument if semNb is out of range.
         */
        int force_unlock_semaphore(int semNb) {
            if(semNb < 0 || semNb >= IMAGE_NB_SEMAPHORE)
                throw std::invalid_argument("Invalid semaphore number " + std::to_string(semNb) + 
                                          ", must be 0-" + std::to_string(IMAGE_NB_SEMAPHORE-1));
            
            return daoShmForceUnlockSemaphore(&image_, semNb);
        }

        /**
         * @brief Clean up semaphore registrations from dead processes.
         * 
         * Scans the registry and removes entries for processes that are no longer running.
         * This helps prevent semaphore exhaustion from crashed processes.
         * 
         * @return Number of stale entries cleaned up, or negative error code.
         */
        int cleanup_stale_semaphores() {
            return daoShmCleanupStaleSemaphores(&image_);
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
       bool is_writer_;  // Track if we created (writer) or opened (reader)

    };
};
#endif // DAO_SHM_HPP
