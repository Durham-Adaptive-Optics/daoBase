#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <signal.h>
#include <unistd.h>
#include <daoLog.hpp>
#include <daoComponent.hpp>

// Define a custom test component that extends the real Dao::Component
class TestComponent : public Dao::Component 
{
public:
    TestComponent(std::string name, Dao::Log::Logger& logger, std::string ip, int port, bool autoRun = false, int core = -1)
        : Dao::Component(name, logger, ip, port, autoRun, core)
        , m_standby_entered(false)
        , m_idle_entered(false)
        , m_running_entered(false)
    {
        // Custom initialization
    }

    // Track state transitions
    void entry_Standby() override 
    {
        Dao::StateMachine::entry_Standby();
        m_standby_entered = true;
        m_log.Info("TestComponent: Standby state entered");
    }

    void entry_Idle() override 
    {
        Dao::StateMachine::entry_Idle();
        m_idle_entered = true;
        m_log.Info("TestComponent: Idle state entered");
    }

    void entry_Running() override
    {
        Dao::StateMachine::entry_Running();
        m_running_entered = true;
        m_log.Info("TestComponent: Running state entered");
    }

    // Track transition events
    void transition_Off_Standby() override
    {
        Dao::StateMachine::transition_Off_Standby();
        m_log.Info("TestComponent: Off->Standby transition");
    }

    void transition_Standby_Idle() override
    {
        Dao::StateMachine::transition_Standby_Idle();
        m_log.Info("TestComponent: Standby->Idle transition");
    }

    void transition_Idle_Running() override
    {
        Dao::StateMachine::transition_Idle_Running();
    }

    // Methods to check if states were entered
    bool isStandbyEntered() const { return m_standby_entered; }
    bool isIdleEntered() const { return m_idle_entered; }
    bool isRunningEntered() const { return m_running_entered; }

private:
    bool m_standby_entered;
    bool m_idle_entered;
    bool m_running_entered;
};

// Declare our global signal handler flag
volatile bool system_running = true;

void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    system_running = false;
}

int main(int argc, char **argv) {
    using namespace std::chrono_literals;
    
    // Create a logger with SCREEN destination to avoid network issues
    std::string name = "AutoTest";
    Dao::Log::Logger logger(name, Dao::Log::Logger::DESTINATION::SCREEN);
    logger.SetLevel(Dao::Log::LEVEL::TRACE);
    
    // Set up network parameters
    std::string ip = "127.0.0.1";
    int port = 5556;  // Using different port to avoid conflict
    
    std::cout << "Creating TestComponent with autoRun = true" << std::endl;
    
    // Create component with autoRun = true
    TestComponent component(name, logger, ip, port, true);
    
    // Set up signal handling
    signal(SIGINT, signalHandler);
    
    while (system_running) {

        std::this_thread::sleep_for(1s);
    }
    
    return 0;
}