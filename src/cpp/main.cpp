#include <signal.h>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <iostream>
#include <daoLog.hpp>

volatile bool system_running = true;
void signalHandler( int signum ) {
   std::cout << "Interrupt signal (" << signum << ") received.\n";
   system_running = false;
}


int main(int argc, char * argv[])
{
    std::string name = "test Comp";
    std::string ip = "127.0.0.1";
    int port = 5555;
    Dao::Log::Logger log(name, Dao::Log::Logger::DESTINATION::NETWORK, ip, port);

    log.SetLevel(Dao::Log::LEVEL::TRACE);

    signal(SIGINT, signalHandler);  
    while(system_running)
    {
        log.Trace("test - %s", "message");
        std::this_thread::sleep_for(std::chrono::microseconds(1000000));
    }

}