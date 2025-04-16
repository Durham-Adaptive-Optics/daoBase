#include <gtest/gtest.h>
#include <daoShmIfce.hpp>
#include <vector>
#include <thread>
#include <chrono>

class DaoShmIfceTest : public ::testing::Test {
protected:
    void SetUp() override {
        logger = new Dao::Log::Logger("TestLogger", Dao::Log::Logger::DESTINATION::SCREEN);
        logger->SetLevel(Dao::Log::LEVEL::TRACE);
    }

    void TearDown() override {
        delete logger;
    }

    Dao::Log::Logger* logger;
    const std::string test_shm_name = "test_shm";
};

// Test creation of new shared memory
TEST_F(DaoShmIfceTest, CreateNewSharedMemory) {
    std::vector<float> test_data(2, 1.234f);
    
    // Create new shared memory
    Dao::ShmIfce<float>* shm = nullptr;
    ASSERT_NO_THROW({
        shm = new Dao::ShmIfce<float>(*logger, test_shm_name, test_data.data(), test_data.size());
    });
    
    shm->set_data(test_data.data(), test_data.size());
    ASSERT_NE(shm, nullptr);
    
    // Verify data was written correctly
    float* data = shm->get_data();
    ASSERT_NE(data, nullptr);
    
    for (size_t i = 0; i < test_data.size(); ++i) {
        EXPECT_FLOAT_EQ(data[i], test_data[i]);
    }
    
    delete shm;
}

// Test connecting to existing shared memory
TEST_F(DaoShmIfceTest, ConnectToExistingSharedMemory) {
    const size_t data_size = 100;
    std::vector<float> test_data(data_size, 2.345f);
    
    // First create the shared memory
    {
        Dao::ShmIfce<float> shm(*logger, test_shm_name, test_data.data(), test_data.size());
    }
    
    // Now try to connect to it
    Dao::ShmIfce<float>* shm_connect = nullptr;
    ASSERT_NO_THROW({
        shm_connect = new Dao::ShmIfce<float>(*logger, test_shm_name);
    });
    
    ASSERT_NE(shm_connect, nullptr);
    
    // Verify data can be read
    float* data = shm_connect->get_data();
    ASSERT_NE(data, nullptr);
    
    for (size_t i = 0; i < data_size; ++i) {
        EXPECT_FLOAT_EQ(data[i], test_data[i]);
    }
    
    delete shm_connect;
}

// Test data update and counter increment
TEST_F(DaoShmIfceTest, UpdateDataAndCounter) {
    const size_t data_size = 10;
    std::vector<float> initial_data(data_size, 1.0f);
    std::vector<float> updated_data(data_size, 2.0f);
    
    Dao::ShmIfce<float> shm(*logger, test_shm_name, initial_data.data(), initial_data.size());
    
    uint64_t initial_counter = shm.get_counter();
    
    // Update data
    shm.set_data(updated_data.data(), updated_data.size());
    
    // Check counter incremented
    EXPECT_GT(shm.get_counter(), initial_counter);
    
    // Verify data was updated
    float* data = shm.get_data();
    ASSERT_NE(data, nullptr);
    
    for (size_t i = 0; i < data_size; ++i) {
        EXPECT_FLOAT_EQ(data[i], updated_data[i]);
    }
}

// Test spin wait functionality
TEST_F(DaoShmIfceTest, SpinWait) {
    const size_t data_size = 10;
    std::vector<float> initial_data(data_size, 1.0f);
    std::vector<float> updated_data(data_size, 3.0f);
    
    Dao::ShmIfce<float> shm(*logger, test_shm_name, initial_data.data(), initial_data.size());
    
    uint64_t initial_counter = shm.get_counter();
    
    // Start a thread that will update the data after a delay
    std::thread update_thread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        shm.set_data(updated_data.data(), updated_data.size());
    });
    
    // Wait for the update using spin wait
    float* data = shm.get_data_spin();
    ASSERT_NE(data, nullptr);
    
    // Verify data was updated
    for (size_t i = 0; i < data_size; ++i) {
        EXPECT_FLOAT_EQ(data[i], updated_data[i]);
    }
    
    // Verify counter increased
    EXPECT_GT(shm.get_counter(), initial_counter);
    
    update_thread.join();
}

// Test timeout functionality
TEST_F(DaoShmIfceTest, Timeout) {
    const size_t data_size = 10;
    std::vector<float> initial_data(data_size, 1.0f);
    
    Dao::ShmIfce<float> shm(*logger, test_shm_name, initial_data.data(), initial_data.size());
    
    // Try to get data with a very short timeout
    float* data = shm.get_data(true, 1, 0.001);
        
    // Try to get data with a very short timeout
    data = shm.get_data(true, 1, 0.001);
    
    // Since no update occurred, this should timeout and return nullptr
    EXPECT_EQ(data, nullptr);
}