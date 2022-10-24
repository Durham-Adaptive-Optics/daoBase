/**
 * @file    daoThreadSafeQueue.hpp
 * @brief   thread safe queue definition
 *
 *
 * @author  D. Barr
 * @date    10 August 2022
 *
 * @bug No known bugs.
 *
 */
#ifndef DAO_THREAD_SAFE_QUEUE_HPP
#define DAO_THREAD_SAFE_QUEUE_HPP

#ifndef __cplusplus
#error This is a C++ include file and cannot be used from plain C
#endif

#include <mutex>
#include <queue>
#include <optional>


namespace Dao
{

    // TODO: Write custom allocator for numa awareness
    // Threads can write in place and the the log thread will retrive the logs
    template<class T> //, class Allocator = std::allocator<T> >
    class ThreadSafeQueue
    {
        public:
            ThreadSafeQueue() = default;
            ThreadSafeQueue(const ThreadSafeQueue<T> &) = delete ;
            ThreadSafeQueue& operator=(const ThreadSafeQueue<T> &) = delete ;

            ThreadSafeQueue(ThreadSafeQueue<T>&& other)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_queue = std::move(other.m_queue);
            }
            
            virtual ~ThreadSafeQueue(){}
            
            size_t size() const
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                return m_queue.size();
            }
            
            std::optional<T> pop()
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_queue.empty())
                {
                    return {};
                }
                T retVal = m_queue.front();
                m_queue.pop();
                return retVal;
            }
            
            // push with lock and copy
            void push(const T &item)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_queue.push(item);
            }
                    
        private:
            inline bool empty() const
            {
                return m_queue.empty();
            }

            std::queue<T> m_queue; //,Allocator> m_queue;
            mutable std::mutex m_mutex;
    };
}; // namespace DAO

#endif /* DAO_THREAD_SAFE_QUEUE_HPP */