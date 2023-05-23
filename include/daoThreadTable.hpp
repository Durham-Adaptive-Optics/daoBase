

#ifndef DAO_THREAD_TABLE_HPP
#define DAO_THREAD_TABLE_HPP

/** 
 *  @file   daoThreadTable.hpp 
 *  @brief  Class for thread Table library
 *  @author dbarr 
 *  @date   2022-07-28 
 ***********************************************/


#include <daoThread.hpp>
#include <daoThreadIfce.hpp>
#include <vector>

namespace Dao
{
    //!  ThreadTable class 
    /*!
    Class to handle multiple thread interfaces and control
    */
    class ThreadTable
    {
        public:
            ThreadTable()
            : m_threads(0)
            {};


            ~ThreadTable()
            {};


            // utility functions for controlling threads
            void Add(ThreadIfce * newThread)
            {
                m_threads.push_back(newThread);
            };

            void Start()
            {
                for (auto thread = begin (m_threads); thread != end (m_threads); ++thread)
                {
                    (*thread)->Start ();
                }
            };

            void Stop()
            {
                for (auto thread = begin (m_threads); thread != end (m_threads); ++thread)
                {
                    (*thread)->Stop ();
                }
            };

            void Exit()
            {
                for (auto thread = begin (m_threads); thread != end (m_threads); ++thread)
                {
                    (*thread)->Exit ();
                }
            }

            void Spawn()
            {
                for (auto thread = begin (m_threads); thread != end (m_threads); ++thread)
                {
                    (*thread)->Spawn ();
                }
            }

            void Join()
            {
                for (auto thread = begin (m_threads); thread != end (m_threads); ++thread)
                {
                    (*thread)->Join ();
                }
            };

            void Kill(int signal)
            {
                for (auto thread = begin (m_threads); thread != end (m_threads); ++thread)
                {
                    (*thread)->Kill (signal);
                }
            }


            // matches API of Signal table to propagate signals to all threads 
            void Signal( int index )
            {
                for (auto thread = begin (m_threads); thread != end (m_threads); ++thread)
                {
                    (*thread)->Signal(index);
                }
            };

            void SignalWaitSpin( int index )
            {
                for (auto thread = begin (m_threads); thread != end (m_threads); ++thread)
                {
                    (*thread)->SignalWaitSpin(index);
                }
            };

            void SignalWaitSleep( int index, int usleeppar )
            {
                for (auto thread = begin (m_threads); thread != end (m_threads); ++thread)
                {
                    (*thread)->SignalWaitSleep(index, usleeppar);
                }
            };
        private:
            std::vector<ThreadIfce*> m_threads;

    };
}; //close namesapace Dao

#endif // DAO_THREAD_TABLE_HPP