#include <gtest/gtest.h>
#include <string>
#include <chrono>
#include <thread>
#include <cstring>

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

class UpdateThreadTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char* shmFile = "/tmp/test.im.shm";
        // Initialize 1D array with 100 elements
        IMAGE* image = nullptr;
        int result = daoShmInit1D(shmFile, 100, &image);
        ASSERT_EQ(result, DAO_SUCCESS) << "Failed to create shared memory file";
        
        if (image) {
            daoShmCloseShm(image);
            free(image);
        }
    }

    void TearDown() override {
        // Clean up the shared memory file
        remove("/tmp/test.im.shm");
    }
};

TEST_F(UpdateThreadTest, set_up)
{
    std::string filename = "/tmp/test.im.shm";
    using namespace std::literals::chrono_literals;
    std::string name = "Test";
    Dao::Log::Logger * logger = new Dao::Log::Logger(name, Dao::Log::Logger::DESTINATION::SCREEN);
    test_class * t = new test_class(name, *logger, 0,0);

    Dao::ShmIfce<float> * shm = new Dao::ShmIfce<float>(*logger);
    IMAGE* img = (IMAGE*)malloc(sizeof(IMAGE));
    memset(img, 0, sizeof(IMAGE)); // Initialize the IMAGE structure
    // Call OpenShm with the string filename and IMAGE pointer
    shm->OpenShm(filename.c_str(), img, -1);  // Added -1 as the default node parameter
    Dao::DoubleBuffer<float>* buffer = new Dao::DoubleBuffer<float>(100);
    
    t->add(shm,buffer,"test map");
    t->Spawn();
    t->Start();
    std::this_thread::sleep_for(10ms);
    EXPECT_EQ(t->isSpawned(), true);
    EXPECT_EQ(t->isRunning(), true);

    t->Stop();
    EXPECT_EQ(t->isSpawned(), true);
    t->Join();

    delete t;
    delete buffer;
    delete shm;
    delete logger;
    free(img);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}