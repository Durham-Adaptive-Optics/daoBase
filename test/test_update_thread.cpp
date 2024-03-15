#include <gtest/gtest.h>
#include <string>


#include <chrono>
#include <thread>

#include <daoLog.hpp>
#include <daoThread.hpp>
#include <daoShmIfce.hpp>
#include <daoDoubleBuffer.hpp>
#include <daoComponentUpdateThread.hpp>

class test_class : public Dao::ComponentUpdateThread
{
    public:
        test_class(std::string name, Dao::Log::Logger& log, int core, int handle)
        : ComponentUpdateThread(name, log, core, handle)
        {

        }
};


// TODO: expand testing
TEST(Restart, set_up)
{
    std::string filename = "/tmp/test.im.shm";
    using namespace std::literals::chrono_literals;
    std::string name = "Test";
    Dao::Log::Logger * logger = new Dao::Log::Logger(name, Dao::Log::Logger::DESTINATION::SCREEN); //, "/tmp/test_update.txt");
    test_class * t = new test_class(name, *logger, 0,0);

    Dao::ShmIfce<float> * shm = new Dao::ShmIfce<float>(*logger);
    IMAGE* img = (IMAGE*)malloc(sizeof(IMAGE));
    shm->OpenShm(filename,img);
    Dao::DoubleBuffer<float>* buffer = new Dao::DoubleBuffer<float>(100);
    
    t->add<float>(shm,buffer,"test map");
//     // bring online
    t->Spawn();
    t->Start();
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(t->isSpawned(), true);
    EXPECT_EQ(t->isRunning(), true);


//     t->Stop();
//     EXPECT_EQ(t->isSpawned(), true);

// // TODO:: fix m_running
//     t->Start();
    t->Stop();
    EXPECT_EQ(t->isSpawned(), true);
    t->Join();

    delete t;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}