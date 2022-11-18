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
    using namespace std::chrono_literals;
    std::string name = "test Comp";

    // frame constants
    std::string raw_pix_name  = "/tmp/raw_pix.im.shm";
    std::string cal_pix_name  = "/tmp/cal_pix.im.shm";
    std::string slopes_name   = "/tmp/slopes.im.shm";

    size_t timeout_packet = 50;
    size_t timeout_frame = 5000;

    size_t length = 57600;
    size_t nPackets = 7;

    Dao::Log::Logger log(name, Dao::Log::Logger::DESTINATION::SCREEN);
    log.SetLevel(Dao::Log::LEVEL::TRACE);

    IMAGE * raw_pix_img = (IMAGE*) malloc(sizeof(IMAGE));
    IMAGE * cal_pix_img = (IMAGE*) malloc(sizeof(IMAGE));
    IMAGE * slopes_img  = (IMAGE*) malloc(sizeof(IMAGE));
    Dao::ShmIfce<uint16_t>  * raw_pix = new Dao::ShmIfce<uint16_t>(log);
    Dao::ShmIfce<float>     * cal_pix = new Dao::ShmIfce<float>(log);
    Dao::ShmIfce<float>     * slopes  = new Dao::ShmIfce<float>(log);

    uint16_t * pixels = new uint16_t[length];
    for(int i = 0; i < length; i++)
    {
        pixels[i] = 2;
    }

    uint64_t * streamAgenda = new uint64_t[nPackets];    
    streamAgenda[0] = 8800;
    streamAgenda[1] = 8800;
    streamAgenda[2] = 8800;
    streamAgenda[3] = 8800;
    streamAgenda[4] = 8800;
    streamAgenda[5] = 8800;
    streamAgenda[6] = 4800;
    
    raw_pix->OpenShm(raw_pix_name, raw_pix_img);
    cal_pix->OpenShm(cal_pix_name, cal_pix_img);
    slopes->OpenShm(slopes_name, slopes_img);
    log.Debug("raw_pix: Size: %zu [%zu]", length, raw_pix->Size());
    log.Debug("cal_pix: Size: %zu [%zu]", length, cal_pix->Size());
    log.Debug("slopes : Size: %zu [%zu]", length, slopes->Size());
    raw_pix->StreamWriteFrameConfigure(nPackets);
    raw_pix->CleanAllSemaphores();
    cal_pix->CleanAllSemaphores();
    slopes ->CleanAllSemaphores();

    // cal_pix->CleanAllSemaphores();

    int frame = 0;
    int frameId = 0;
    
    //sleep(1);
    while(system_running)
    {
        pixels[0] = frame;
        log.Debug("Sending Frame: %zu [%zu]", frame, raw_pix->GetFrameCounter() + 1);
        size_t pos = 0;
        // sleep(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_frame));
        raw_pix->StreamWriteFrameStart();
        for(size_t i = 0; i < nPackets; i++)
        {
            //log.Debug("Sending packet: %zu of %zu", i , nPackets);
            raw_pix->StreamWriteFramePacket(pixels, pos, streamAgenda[i], i);
            pos += streamAgenda[i];
            std::this_thread::sleep_for(std::chrono::milliseconds(timeout_packet));
            //sleep(1);
        }
        raw_pix->StreamWriteFrameFinalise(frame);

        // cal_pix->FullFrameReadData();
        // sleep(1);
        frame++;



        // do I wait for the semaphore from cal pix
    }
    raw_pix->CloseShm();
    cal_pix->CloseShm();

    delete [] pixels;
    delete [] streamAgenda;
    free(raw_pix_img);
    free(cal_pix_img);
}