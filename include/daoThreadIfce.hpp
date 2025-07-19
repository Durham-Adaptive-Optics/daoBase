#ifndef DAO_THREADS_IFCE_HPP
#define DAO_THREADS_IFCE_HPP

/** 
 *  @file   daoThreadIfce.hpp 
 *  @brief  Class for threading Ifce
 *  @author dbarr 
 *  @date   2022-07-28 
 ***********************************************/



namespace Dao
{
    //!  daoThreadIfce class 
    /*!
    Class to interface to the thread class
    */
   class ThreadIfce
   {
     public:
        virtual ~ThreadIfce(){};
        
        virtual void Start()    = 0;
        virtual void Stop()     = 0;    
        virtual void Exit()     = 0;
        virtual void Spawn()    = 0;
        virtual void Join()     = 0;
        virtual void Kill(int signal) = 0;    
        
        virtual bool isSpawned() = 0;
         virtual bool isRunning() = 0;
      // some signals
         virtual void Signal(int index) = 0;
         virtual void SignalWaitSpin(int index) = 0;
         virtual void SignalWaitSleep(int index, int usleep) = 0;
   };
 
}; // closes namespace Dao

#endif // DAO_THREADS_IFCE_HPP