/**
 * @ Author: GitHub Copilot
 * @ Company: Centre for Advanced Instrumentation, Durham University
 * @ Create Time: 2026-01-26
 * @ Description: Tests for semaphore registry functionality
 */

#include <gtest/gtest.h>
#include <daoShm.hpp>
#include <filesystem>
#include <thread>
#include <vector>
#include <chrono>
#include <sstream>

extern "C" {
#include <dao.h>
}

/**
 * @brief Test fixture for semaphore registry tests
 */
class SemaphoreRegistryTest : public ::testing::Test
{
protected:
    std::string shmPath_;
    
    void SetUp() override
    {
        std::stringstream ss;
        const auto testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        // Use shorter names to avoid macOS 31-character semaphore name limit
        std::string testName = testInfo->name();
        std::string shortName = testName.substr(0, 8);
        ss << "/tmp/sr_" << shortName << ".im.shm";
        shmPath_ = ss.str();
    }
    
    void TearDown() override
    {
        if (std::filesystem::exists(shmPath_)) {
            std::filesystem::remove(shmPath_);
        }
    }
};

/**
 * @brief Test single reader registration
 */
TEST_F(SemaphoreRegistryTest, SingleReaderRegistration)
{
    // Set log level to debug to see validation messages
    daoSetLogLevel(5);  // DEBUG level
    
    Dao::Shm<float> writer(shmPath_, {10, 10});
    Dao::Shm<float> reader(shmPath_);
    
    // Register reader with custom name
    int sem_index = reader.register_reader("test_reader", -1);
    ASSERT_GE(sem_index, 0);
    ASSERT_LT(sem_index, IMAGE_NB_SEMAPHORE);
    
    // Small delay to ensure shared memory write is visible
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Validate registration
    ASSERT_TRUE(reader.validate_registration());
    
    // Unregister
    ASSERT_EQ(reader.unregister_reader(), 0);
    ASSERT_FALSE(reader.validate_registration());
}

/**
 * @brief Test multiple readers registering for different semaphores
 */
TEST_F(SemaphoreRegistryTest, MultipleReaders)
{
    Dao::Shm<float> writer(shmPath_, {10, 10});
    
    std::vector<std::unique_ptr<Dao::Shm<float>>> readers;
    std::vector<int> semaphores;
    
    // Register 5 readers
    for (int i = 0; i < 5; i++) {
        auto reader = std::make_unique<Dao::Shm<float>>(shmPath_);
        std::string name = "reader_" + std::to_string(i);
        int sem = reader->register_reader(name.c_str(), -1);
        
        ASSERT_GE(sem, 0);
        ASSERT_LT(sem, IMAGE_NB_SEMAPHORE);
        semaphores.push_back(sem);
        readers.push_back(std::move(reader));
    }
    
    // Check all semaphores are different
    for (size_t i = 0; i < semaphores.size(); i++) {
        for (size_t j = i + 1; j < semaphores.size(); j++) {
            ASSERT_NE(semaphores[i], semaphores[j]);
        }
    }
    
    // Small delay to ensure shared memory writes are visible
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // All readers should validate
    for (auto& reader : readers) {
        ASSERT_TRUE(reader->validate_registration());
    }
    
    // Get registered readers
    auto registry = writer.get_registered_readers();
    ASSERT_EQ(registry.size(), 5);
    
    // Unregister all
    for (auto& reader : readers) {
        ASSERT_EQ(reader->unregister_reader(), 0);
    }
    
    registry = writer.get_registered_readers();
    ASSERT_EQ(registry.size(), 0);
}

/**
 * @brief Test semaphore exhaustion (all 10 slots used)
 */
TEST_F(SemaphoreRegistryTest, SemaphoreExhaustion)
{
    Dao::Shm<float> writer(shmPath_, {10, 10});
    
    std::vector<std::unique_ptr<Dao::Shm<float>>> readers;
    
    // Register 10 readers (max)
    for (int i = 0; i < IMAGE_NB_SEMAPHORE; i++) {
        auto reader = std::make_unique<Dao::Shm<float>>(shmPath_);
        std::string name = "reader_" + std::to_string(i);
        int sem = reader->register_reader(name.c_str(), -1);
        ASSERT_GE(sem, 0);
        readers.push_back(std::move(reader));
    }
    
    // Try to register 11th reader - should fail
    Dao::Shm<float> extra_reader(shmPath_);
    ASSERT_THROW(extra_reader.register_reader("extra", -1), std::runtime_error);
    
    // Unregister one reader
    ASSERT_EQ(readers[5]->unregister_reader(), 0);
    
    // Now extra reader should succeed
    ASSERT_NO_THROW(extra_reader.register_reader("extra", -1));
}

/**
 * @brief Test preferred semaphore selection
 */
TEST_F(SemaphoreRegistryTest, PreferredSemaphore)
{
    Dao::Shm<float> writer(shmPath_, {10, 10});
    Dao::Shm<float> reader(shmPath_);
    
    // Request specific semaphore (5)
    int sem = reader.register_reader("test_reader", 5);
    ASSERT_EQ(sem, 5);
    
    // Try to register another reader for same semaphore
    Dao::Shm<float> reader2(shmPath_);
    int sem2 = reader2.register_reader("test_reader2", 5);
    // Should get a different semaphore
    ASSERT_NE(sem2, 5);
}

/**
 * @brief Test cleanup of stale semaphores
 */
TEST_F(SemaphoreRegistryTest, StaleCleanup)
{
    Dao::Shm<float> writer(shmPath_, {10, 10});
    
    // Create and register reader in a scope
    {
        Dao::Shm<float> reader(shmPath_);
        reader.register_reader("temp_reader", -1);
        // Reader destructor should unregister automatically
    }
    
    // Check registry is empty
    auto registry = writer.get_registered_readers();
    ASSERT_EQ(registry.size(), 0);
}

/**
 * @brief Test force unlock functionality
 */
TEST_F(SemaphoreRegistryTest, ForceUnlock)
{
    Dao::Shm<float> writer(shmPath_, {10, 10});
    Dao::Shm<float> reader(shmPath_);
    
    int sem = reader.register_reader("test_reader", -1);
    ASSERT_GE(sem, 0);
    
    // Force unlock the semaphore
    ASSERT_EQ(writer.force_unlock_semaphore(sem), 0);
    
    // Reader validation should now fail
    ASSERT_FALSE(reader.validate_registration());
    
    // Test invalid semaphore number
    ASSERT_THROW(writer.force_unlock_semaphore(-1), std::invalid_argument);
    ASSERT_THROW(writer.force_unlock_semaphore(IMAGE_NB_SEMAPHORE), std::invalid_argument);
}

/**
 * @brief Test registry information retrieval
 */
TEST_F(SemaphoreRegistryTest, RegistryInfo)
{
    Dao::Shm<float> writer(shmPath_, {10, 10});
    
    std::vector<std::unique_ptr<Dao::Shm<float>>> readers;
    std::vector<std::string> names = {"alice", "bob", "charlie"};
    
    for (const auto& name : names) {
        auto reader = std::make_unique<Dao::Shm<float>>(shmPath_);
        reader->register_reader(name.c_str(), -1);
        readers.push_back(std::move(reader));
    }
    
    // Get registry info
    auto registry = writer.get_registered_readers();
    ASSERT_EQ(registry.size(), 3);
    
    // Check that all names are present
    for (const auto& name : names) {
        bool found = false;
        for (const auto& entry : registry) {
            std::string entry_name(entry.reader_name);
            if (entry_name == name) {
                found = true;
                ASSERT_TRUE(entry.locked);
                ASSERT_GT(entry.reader_pid, 0);
                break;
            }
        }
        ASSERT_TRUE(found) << "Reader " << name << " not found in registry";
    }
}

/**
 * @brief Test registration during concurrent writes
 */
TEST_F(SemaphoreRegistryTest, ConcurrentRegistration)
{
    Dao::Shm<float> writer(shmPath_, {10, 10});
    
    std::vector<std::thread> threads;
    std::vector<int> semaphores(5, -1);
    std::atomic<int> success_count{0};
    
    // Start 5 threads that each register a reader
    for (int i = 0; i < 5; i++) {
        threads.emplace_back([this, i, &semaphores, &success_count]() {
            try {
                Dao::Shm<float> reader(shmPath_);
                std::string name = "concurrent_" + std::to_string(i);
                int sem = reader.register_reader(name.c_str(), -1);
                semaphores[i] = sem;
                success_count++;
                
                // Keep reader alive for a bit
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                reader.unregister_reader();
            } catch (const std::exception& e) {
                // Registration failed
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_EQ(success_count, 5);
    
    // All semaphores should be different
    for (size_t i = 0; i < semaphores.size(); i++) {
        ASSERT_GE(semaphores[i], 0);
        for (size_t j = i + 1; j < semaphores.size(); j++) {
            ASSERT_NE(semaphores[i], semaphores[j]);
        }
    }
}

/**
 * @brief Test statistics tracking (read_count, timeout_count)
 */
TEST_F(SemaphoreRegistryTest, StatisticsTracking)
{
    Dao::Shm<float> writer(shmPath_, {10, 10});
    Dao::Shm<float> reader(shmPath_);
    
    int sem = reader.register_reader("stats_reader", -1);
    ASSERT_GE(sem, 0);
    
    // Perform some reads
    float frame[100];
    for (int i = 0; i < 100; i++) {
        frame[i] = static_cast<float>(i);
    }
    
    for (int i = 0; i < 5; i++) {
        writer.set_frame(frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Check registry for statistics
    auto registry = writer.get_registered_readers();
    ASSERT_EQ(registry.size(), 1);
    
    // Note: read_count will be 0 unless we actually call get_frame with semaphore wait
    // This is just checking the structure is accessible
    ASSERT_EQ(registry[0].read_count, 0);
    ASSERT_EQ(registry[0].timeout_count, 0);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
