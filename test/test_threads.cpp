#include <gtest/gtest.h>
#include <string>
#include <daoThread.hpp>
#include <daoLog.hpp>
#include <chrono>
#include <thread>

class test_class : public Dao::Thread 
{
    public:
        test_class(std::string name, Dao::Log::Logger& log, int core, int handle)
        : Dao::Thread(name, log, core, handle)
        {
            frame = 0;
        }

    protected:
        void OnceOnSpawn(){printf("OnceOnSpawn()\n");};
        void OnceOnStart(){printf("OnceOnStart()\n");};
        void OnceOnStop(){printf("OnceOnStop()\n");};
        void OnceOnExit(){printf("OnceOnExit()\n");};

        void RestartableThread() override
        {
            frame++;
        }
        int frame;
};

// TODO: expand testing
TEST(Create, set_up) {
    std::string name = "Test";
    Dao::Log::Logger * logger = new Dao::Log::Logger(name, Dao::Log::Logger::DESTINATION::SCREEN);
    test_class * t = new test_class("Test", *logger, 0,0);

    // bring online
    t->Spawn();
    t->Start();

    EXPECT_EQ(t->isSpawned(), true);

    t->Join();

    delete t;
}


// TODO: expand testing
TEST(Restart, set_up) {
    using namespace std::literals::chrono_literals;
    std::string name = "Test";
    Dao::Log::Logger * logger = new Dao::Log::Logger(name, Dao::Log::Logger::DESTINATION::SCREEN);
    test_class * t = new test_class(name, *logger, 0,0);

    // bring online
    t->Spawn();
    t->Start();

    EXPECT_EQ(t->isSpawned(), true);
    EXPECT_EQ(t->isRunning(), true);

    t->Stop();
    EXPECT_EQ(t->isSpawned(), true);

    std::this_thread::sleep_for(20ms);
// TODO:: fix m_running
    t->Start();

    std::this_thread::sleep_for(20ms);
    t->Join();

    delete t;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}