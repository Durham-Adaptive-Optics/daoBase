#ifndef DAO_THREAD_BASE_HPP
#define DAO_THREAD_BASE_HPP

/** 
 *  @file   daoThreadBASE.hpp 
 *  @brief  Class for threading library
 *  @author dbarr 
 *  @date   2022-07-28 
 ***********************************************/


#include <daoSignalTable.hpp>
#include <daoNuma.hpp>
#include <daoLog.hpp>

#include <thread>
#include <pthread.h>
#include <mutex>
#include <condition_variable>

#include <iostream>
namespace Dao
{
    //!  daoThreadBase class 
    /*!#
    Class for base object of Threads
    */
    class ThreadBase
    {
        public:
            /**
             * Create the default Thread class.
             * @brief Default constructor.
             * @param
             */
            ThreadBase(std::string thread_name, Log::Logger& logger, int core=-1, int thread_number=-1)
            : m_thread_name(thread_name)
            , m_log(logger)
            , m_core(core)
            , m_node(-1)
            , m_thread_number(thread_number)
            , m_start(false)
            , m_running(false)
            , m_spawned(false)
            , m_stop(true)
            , m_start_waiting(false)
            {
                m_thread_name.resize(15);
                // calculate node from core...
                if (m_core >= 0)
                {
                    m_node = Numa::Core2Node(m_core);
                }
                m_signal_table = new SignalTable;
            }

            virtual ~ThreadBase()
            {
                delete m_signal_table;
            };

            /**
             * Create a new SignalTable object with default number of signals 256.
             * @brief Default constructor.
             * @param
             */
            void Start()
            {
                m_running = true;
                m_stop = false;
                    
                m_start_mutex.lock();
                m_start = true;
                m_start_condition.notify_one();
                m_start_mutex.unlock();
            };


            /**
             * Create a new SignalTable object with default number of signals 256.
             * @brief Default constructor.
             * @param
             */
            void Stop()
            {
                m_stop = true;
            };

            /**
             * Create a new SignalTable object with default number of signals 256.
             * @brief Default constructor.
             * @param
             */
            void Exit()
            {
                m_stop = true;
                m_running = false;  

                m_start_mutex.lock();
                if (m_start_waiting == true)
                    m_start = true;
                m_start_condition.notify_one();
                m_start_mutex.unlock();
            }

            /**
             * Create a new SignalTable object with default number of signals 256.
             * @brief Default constructor.
             * @param
             */
            void Spawn()
            {
                size_t core = Numa::GetProcAffinity();
                m_log.Trace("%s: running on core: %zu", m_thread_name.c_str(), core);
                if(m_core >= 0)
                {
                    m_node = Numa::Core2Node(m_core);
                    m_log.Trace("%s: Moving to core : %zu", m_thread_name.c_str(), m_core);
                    Numa::SetProcAffinity(m_core);
                }
                m_log.Trace("%s: spawning", m_thread_name.c_str());
                m_thread = std::thread{&ThreadBase::threadEntryPoint, this};
                if(m_core >=0)
                {
                    m_log.Trace("%s: reverting to core %zu to wait for signal", m_thread_name.c_str(), core);
                    Numa::SetProcAffinity(core);
                }
                usleep(200);
                // wait for signal to say spawn has completed.
                m_signal_table->SignalReceiveSpin(SIGNAL_THREAD_READY);
                m_spawned = true;
            };


            /**
             * Create a new SignalTable object with default number of signals 256.
             * @brief Default constructor.
             * @param
             */
            void Join()
            {
                if(!m_stop || m_running)
                {
                    Stop();
                    Exit();
                }

                m_thread.join();
                m_spawned = false;
            };

            /**
             * Create a new SignalTable object with default number of signals 256.
             * @brief Default constructor.
             * @param
             */
            void Kill(int signal)
            {
                // std::terminate();
            };

            virtual void Body() = 0; // overwritten later

            // get status stuff
            inline bool isRunning(){return m_running;};
            inline bool isSpawned(){return m_spawned;};
            std::string getThreadName(){return m_thread_name;};
            int getAffinity(){return m_core;};
            int getNumaNode(){return m_node;};


            // a highspeed signalling table for interThread communication
            SignalTable * m_signal_table;

        protected:
            void threadEntryPoint()
            {
                m_log.Trace("%s: spawned", m_thread_name.c_str());
                if(m_core >=0)
                {
                    Dao::Numa::SetProcAffinity(m_core);
                }
                if(m_thread_name != "")
                {
                    m_log.Trace("Setting thread name to: %s", m_thread_name.c_str() );
                
                    int rc = pthread_setname_np( pthread_self(), m_thread_name.c_str());
                    if(rc != 0)
                        std::cout << "Return : " << rc << std::endl;
                }
                // setting 
                if(getuid() == 0)
                {
                    struct sched_param param;
                    param.sched_priority = 95;
                    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
                }

                try
                {
                    OnceOnSpawn();
                }
                catch(const std::exception& e)
                {
                    std::cerr << e.what() << '\n';
                }
                m_signal_table->SignalSend(SIGNAL_THREAD_READY);

                try
                {
                    internalThreadFunction();
                }
                catch(const std::exception& e)
                {
                    std::cerr << e.what() << '\n';
                }

                try
                {
                    OnceOnExit();
                }
                catch(const std::exception& e)
                {
                    std::cerr << e.what() << '\n';
                }
            }

            void internalThreadFunction()
            {
                do
                {
                    std::unique_lock<std::mutex> lk(m_start_mutex);
                    m_start_waiting = true;
                    while(!m_start)
                        m_start_condition.wait(lk);
                    m_start = false;
                    m_start_waiting = false;
                    lk.unlock();

                    try
                    {
                        OnceOnStart();
                    }
                    catch(const std::exception& e)
                    {
                        std::cerr << e.what() << '\n';
                    }
                    while((m_stop == false) && (m_running == true))
                    {
                        try
                        {
                            this->Body();
                        }
                        catch(const std::exception& e)
                        {
                            std::cerr << e.what() << '\n';
                        }
                    }
                    m_stop = true;

                    try
                    {
                        OnceOnStop();
                    }
                    catch(const std::exception& e)
                    {
                        std::cerr << e.what() << '\n';
                    }
                    
                }
                while(m_running == true);

                m_running = false;
            }


            // a bunch of empty functions that can be used to help configure things in thread.
            virtual void OnceOnSpawn(){m_log.Trace("ComponentBase::OnceOnSpawn()");};
            virtual void OnceOnStart(){m_log.Trace("ComponentBase::OnceOnStart()");};
            virtual void OnceOnStop(){m_log.Trace("ComponentBase::OnceOnStop()");};
            virtual void OnceOnExit(){m_log.Trace("ComponentBase::OnceOnExit()");};
            
            std::string m_thread_name;
            Log::Logger& m_log;
            
            int m_core;
            int m_node;
            int m_thread_number; // used when multiple threads of the same type used.

            volatile bool m_start;
            volatile bool m_running;
            volatile bool m_spawned;
            volatile bool m_stop;
            volatile bool m_start_waiting;

            std::thread m_thread;

            // threading stuff
            std::mutex m_start_mutex;
            std::condition_variable m_start_condition;
    };

} // closes namespace Dao

#endif // DAO_THREADS_HPP