#include <daoComponent.hpp>
#include <iostream>
#include <signal.h>
#include <thread>
#include <chrono>
#include <pthread.h>
#include <errno.h>
#include <cstring>


namespace Dao
{

    /**
     * @brief Convenience function for creating component and running it
     * @tparam ComponentType The specific component class (must inherit from Component)
     * @param config_file Path to YAML configuration file
     * @param autoRun Whether to automatically run the component after creation
     * @return Pointer to created component instance
     */
    void RunComponentLoop(Dao::Log::Logger& m_log)
    {
        // Block SIGINT and SIGTERM for the current thread and all future threads
        sigset_t signal_set;
        sigemptyset(&signal_set);
        sigaddset(&signal_set, SIGINT);
        sigaddset(&signal_set, SIGTERM);
        
        // Block the signals for this thread and all threads it creates
        pthread_sigmask(SIG_BLOCK, &signal_set, nullptr);
        
        // Run the component continuously until a signal is received
        int received_signal;
        bool running = true;
        
        m_log.Info("Component loop started. Waiting for signals...");
        int result = sigwait(&signal_set, &received_signal);
        if (result == 0)
        {
            m_log.Info("Signal received: %s ", strsignal(received_signal));
            running = false;
        }
        else
        {
            m_log.Error("Error waiting for signal: %s", strerror(errno));
        }
        m_log.Info("Component loop ended.");
    }

} // namespace Dao
