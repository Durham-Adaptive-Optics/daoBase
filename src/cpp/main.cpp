#include <signal.h>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <iostream>
#include <daoLog.hpp>
#include <daoShmIfce.hpp>

volatile bool system_running = true;
void signalHandler( int signum ) {
   std::cout << "\nInterrupt signal (" << signum << ") received.\n";
   system_running = false;
}


int main(int argc, char * argv[])
{
    std::string name = "test Comp";

    // frame constants
    std::string shm_name = "/tmp/test.im.shm";
    size_t nPackets = 2;
    size_t length = 10;

    Dao::Log::Logger log(name, Dao::Log::Logger::DESTINATION::SCREEN);
    log.SetLevel(Dao::Log::LEVEL::TRACE);

    IMAGE * shm_img = (IMAGE*) malloc(sizeof(IMAGE));
    Dao::ShmIfce<float> * shm = new Dao::ShmIfce<float>(log);
    
    shm->OpenShm(shm_name, shm_img);

    float * data_ptr = shm->GetPtr();
    signal(SIGINT, signalHandler); 
    shm->CleanAllSemaphores();


    int frame = 0;
    int frameId = 0;

    shm->StreamReadFrameConnect();
    while(system_running)
    {
        frame = shm->GetFrameCounter();
        frameId = shm->GetFrameID();
        std::cout << "Waiting for Frame: " << frameId+1 << "[" << frame+1 << "]" << std::endl;
        shm->StreamReadFrameBegin();
        for(size_t i = 0; i < nPackets; i++)
        {
            // get data
            int iter = 0;
            int r = -1;
            std::cout << "Waiting for packet: " << i << " of " << nPackets << std::endl;
            
            int j = shm->StreamReadFramePacket();
            std::cout << "Received for packet: " << j << " of " << nPackets << std::endl;
            std::cout << "Rec: ";
            for (int i = 0; i < length;  i++)
            {
                std::cout << data_ptr[i] << " ";
            }
            std::cout << std::endl;
        }
        shm->StreamReadFrameFinalise();

        if(!system_running)
            break;     


            
        std::cout << "Rec: ";
        for (int i = 0; i < length;  i++)
        {
            std::cout << data_ptr[i] << " ";
        }
        std::cout << std::endl;
    }



    free(shm_img);
}