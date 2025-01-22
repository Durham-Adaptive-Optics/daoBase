#ifndef DAO_THREAD_HPP
#define DAO_THREAD_HPP

/** 
 *  @file   daoThread.hpp 
 *  @brief  Class for threading library
 *  @author dbarr 
 *  @date   2022-07-28 
 ***********************************************/

#include <daoThreadBase.hpp>
#include <daoThreadIfce.hpp>
#include <daoLog.hpp>


#include <string>

namespace Dao
{
    //!  daoThreadBase class 
    /*!#
    Class to bring together the ThreadBase and Thread Interface
    */
    class Thread: public ThreadBase, public ThreadIfce
    {
        public:
            Thread(std::string name, Log::Logger& logger, int core=-1, int thread_number=-1, bool rt_enabled=true)
            : ThreadBase(name, logger, core, thread_number, rt_enabled)
            {

            }


            virtual void Start()    override
            {
                ThreadBase::Start();
            }


            virtual void Stop()     override
            {
                ThreadBase::Stop();
            }  


            virtual void Exit()     override
            {
                ThreadBase::Exit();
            }


            virtual void Body()  override
            {
                this->RestartableThread();
            }

            virtual void Spawn()    override
            {
                ThreadBase::Spawn();
            }


            virtual void Join()     override
            {
                ThreadBase::Join();
            }


            virtual void Kill(int signal) override
            {
                ThreadBase::Kill(signal);
            }


            virtual void Signal(int index) override
            {
                ThreadBase::m_signal_table->SignalSend(index);
            }


            virtual void SignalWaitSpin(int index) override
            {
                ThreadBase::m_signal_table->SignalReceiveSpin(index);
            }


            virtual void SignalWaitSleep(int index, int usleep) override
            {
                ThreadBase::m_signal_table->SignalReceiveSleep(index, usleep);
            }

        protected:
            virtual void RestartableThread() = 0;       
    };
}; // closes namespace Dao

#endif // DAO_THREADS_HPP