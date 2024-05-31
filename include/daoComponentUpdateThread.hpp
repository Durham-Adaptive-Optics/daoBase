/**
 * @file    daoComponentUpdateThread.hpp
 * @brief   map Update thread definition
 *
 *
 * @author  D. Barr
 * @date    08 August 2022
 *
 * @bug No known bugs.
 *
 */
#ifndef DAO_COMPONENT_UPDATE_THREAD_HPP
#define DAO_COMPONENT_UPDATE_THREAD_HPP

#ifndef __cplusplus
#error This is a C++ include file and cannot be used from plain C
#endif

#include <string>
#include <memory>
#include <map>

#include <daoThread.hpp>
#include <daoShmIfce.hpp>
#include <daoDoubleBuffer.hpp>

namespace Dao
{

    template<class T>
    class ItemUpdate
    {
        public:
            ItemUpdate(ShmIfce<T> * shm, DoubleBuffer<T>* buffer, std::string name)
            : m_shm(shm)
            , m_buffer(buffer)
            , m_name(name)
            , m_counter(0)
            {

            }
            
            ~ItemUpdate(){}

            void check_update(Log::Logger& logger)
            {
                if(m_shm->GetFrameCounter() > m_counter)
                {
                    logger.Info("New map available for %s", m_name.c_str());
                    m_buffer->CopyAndSwap(m_shm->GetPtr());
                    m_counter = m_shm->GetFrameCounter();
                }
            }

            void setCounter()
            {
                m_counter=m_shm->GetFrameCounter();
            }

            uint64_t getCounter()
            {
                return m_counter;
            }

            std::string getName()
            {
                return m_name;
            }


        private:
            ShmIfce<T> * m_shm;
            DoubleBuffer<T>* m_buffer;
            std::string m_name;
            uint64_t m_counter;
    };


    class ComponentUpdateThread : public Thread
    {
        public:
            ComponentUpdateThread(std::string name, Log::Logger& logger, int core=-1, int handle=-1)
            : Thread("Up_"+ name, logger, core, handle)
            {

            }

            ~ComponentUpdateThread()
            {

            }

            template<class T>
            void add(ShmIfce<T> * shm, DoubleBuffer<T> * buffer, std::string name)
            {
                if constexpr (std::is_same<T, int8_t>::value)
                {
                    m_int8.emplace_back(shm,buffer,name);
                    m_log.Debug("Adding %s to m_int8", name.c_str());
                }
                else if constexpr (std::is_same<T, int16_t>::value)
                {
                    m_int16.emplace_back(shm,buffer,name);
                    m_log.Debug("Adding %s to m_int16", name.c_str());
                }
                else if constexpr (std::is_same<T, int32_t>::value)
                {
                   m_int32.emplace_back(shm,buffer,name);
                   m_log.Debug("Adding %s to m_int32", name.c_str());
                }
                else if constexpr (std::is_same<T, int64_t>::value)
                {
                    m_int64.emplace_back(shm,buffer,name);
                    m_log.Debug("Adding %s to m_int64", name.c_str());
                }
                else if constexpr (std::is_same<T, uint8_t>::value)
                {
                    m_uint8.emplace_back(shm, buffer, name);
                    m_log.Debug("Adding %s to m_uint8", name.c_str());
                }
                else if constexpr (std::is_same<T, uint16_t>::value)
                {
                    m_uint16.emplace_back(shm,buffer,name);
                    m_log.Debug("Adding %s to m_uint16", name.c_str());
                }
                else if constexpr (std::is_same<T, uint32_t>::value)
                {
                    m_uint32.emplace_back(shm,buffer,name);
                    m_log.Debug("Adding %s to m_uint32", name.c_str());
                }
                else if constexpr (std::is_same<T, uint64_t>::value)
                {
                    m_uint64.emplace_back(shm,buffer,name);
                    m_log.Debug("Adding %s to m_uint64", name.c_str());
                }
                else if constexpr (std::is_same<T, float>::value)
                {
                    m_float.emplace_back(shm, buffer, name);
                    m_log.Debug("Adding %s to m_float", name.c_str());
                }
                else if constexpr (std::is_same<T, double>::value)
                {
                    m_double.emplace_back(shm, buffer, name);
                    m_log.Debug("Adding %s to m_double", name.c_str());
                }
            }

        protected:
            // these can be used to reset and reconfigure the thread.
            void OnceOnSpawn() override
            {

            };

            void OnceOnStart() override
            {
                for (auto& element : m_int8)
                {
                    element.setCounter();
                    m_log.Debug("%s (int8_t)  - %zu", element.getName().c_str(), element.getCounter());
                }
                for (auto& element : m_int16)
                {
                    element.setCounter();
                    m_log.Debug("%s (int16_t)  - %zu", element.getName().c_str(), element.getCounter());
                }
                for (auto& element : m_int32)
                {
                    element.setCounter();
                    m_log.Debug("%s (int32_t)  - %zu", element.getName().c_str(), element.getCounter());
                }
                for (auto& element : m_int64)
                {
                    element.setCounter();
                    m_log.Debug("%s (int64_t)  - %zu", element.getName().c_str(), element.getCounter());
                }
                for (auto& element : m_uint8)
                {
                    element.setCounter();
                    m_log.Debug("%s (uint8_t)  - %zu", element.getName().c_str(), element.getCounter());
                }
                for (auto& element : m_uint16)
                {
                    element.setCounter();
                    m_log.Debug("%s (uint16_t)  - %zu", element.getName().c_str(), element.getCounter());
                }
                for (auto& element : m_uint32)
                {
                    element.setCounter();
                    m_log.Debug("%s (uint32_t)  - %zu", element.getName().c_str(), element.getCounter());
                }
                for (auto& element : m_uint64)
                {
                    element.setCounter();
                    m_log.Debug("%s (uint64_t)  - %zu", element.getName().c_str(), element.getCounter());
                }
                for (auto& element : m_float)
                {
                    element.setCounter(); 
                    m_log.Debug("%s (float) - %zu", element.getName().c_str(), element.getCounter());
                }
                for (auto& element : m_double)
                {
                    element.setCounter(); 
                    m_log.Debug("%s (double) - %zu", element.getName().c_str(), element.getCounter());
                }
            }

            void OnceOnStop() override
            {

            };

            void OnceOnExit() override
            {

            };

            void RestartableThread() override
            {
                for (auto& element : m_int8)
                {
                    element.check_update(m_log); 
                }
                for (auto& element : m_int16)
                {
                    element.check_update(m_log); 
                } 
                for (auto& element : m_int32)
                {
                    element.check_update(m_log); 
                }
                for (auto& element : m_int64)
                {
                    element.check_update(m_log); 
                }
                for (auto& element : m_uint8)
                {
                    element.check_update(m_log); 
                }
                for (auto& element : m_uint16)
                {
                    element.check_update(m_log); 
                } 
                for (auto& element : m_uint32)
                {
                    element.check_update(m_log); 
                }
                for (auto& element : m_uint64)
                {
                    element.check_update(m_log); 
                }
                for (auto& element : m_float)
                {
                    element.check_update(m_log); 
                }
                for (auto& element : m_double)
                {
                    element.check_update(m_log); 
                }
            }


        private:

            // map for each type
            std::vector<ItemUpdate<int8_t>> m_int8;
            std::vector<ItemUpdate<int8_t>> m_int16;
            std::vector<ItemUpdate<int8_t>> m_int32;
            std::vector<ItemUpdate<int8_t>> m_int64;
            std::vector<ItemUpdate<int8_t>> m_uint8;
            std::vector<ItemUpdate<int8_t>> m_uint16;
            std::vector<ItemUpdate<int8_t>> m_uint32;
            std::vector<ItemUpdate<int8_t>> m_uint64;
            std::vector<ItemUpdate<float>> m_float;
            std::vector<ItemUpdate<float>> m_double;
    }; 

    // template void ComponentUpdateThread::add<int8_t>(ShmIfce<int8_t> * shm, DoubleBuffer<int8_t> * buffer, std::string name);
    // template void ComponentUpdateThread::add<float>(ShmIfce<float> * shm, DoubleBuffer<float> * buffer, std::string name);
    // template void ComponentUpdateThread::add<double>(ShmIfce<double> * shm, DoubleBuffer<double> * buffer, std::string name);
}; // namespace DAO



#endif /* DAO_COMPONENT_UPDATE_THREAD_HPP */