#include <gtest/gtest.h>
#include <string>
#include <daoThread.hpp>
#include <daoThreadTable.hpp>
#include <daoThreadIfce.hpp>
#include <daoLog.hpp>

class test_class : public Dao::Thread 
{
    public:
        test_class(std::string name, Dao::Log::Logger& logger, int core, int handle)
        : Dao::Thread(name, logger, core, handle)
        {
            frame = 0;
        }

    protected:
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
    logger->SetLevel(Dao::Log::LEVEL::TRACE);
    test_class * t1 = new test_class("Test1", *logger, 0,0);
    test_class * t2 = new test_class("Test2", *logger, 0,0);

    Dao::ThreadTable * thread_table = new Dao::ThreadTable();

    // Add classes to thread_block
    thread_table->Add(static_cast<Dao::ThreadIfce*>(t1));
    thread_table->Add(static_cast<Dao::ThreadIfce*>(t2));

    // bring online
    thread_table->Spawn();
    thread_table->Start();

    EXPECT_EQ(t1->isSpawned(), true);
    EXPECT_EQ(t2->isSpawned(), true);


    thread_table->Join();

    delete thread_table;
    sleep(1);
    delete t1;
    delete t2;
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}