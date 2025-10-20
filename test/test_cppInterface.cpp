/**
 * @ Author: Thomas N. Davies
 * @ Company: Centre for Advanced Instrumentation, Durham University
 * @ Contact: thomas.n.davies@durham.ac.uk
 * @ Create Time: 2025-10-17 14:47:41
 * @ Description: Tests for the modern C++ dao shared memory interface.
 */

#include <gtest/gtest.h>
#include <daoShm.hpp>
#include <filesystem>
#include <cstring>
#include <thread>
#include <future>
#include <sstream>

 /**
  * @brief Test fixture for providing and cleaning up a shared memory file path.
  */
class Suite : public ::testing::Test
{
    protected:
    std::string shmPath_;

    void SetUp() override
    {
        std::stringstream ss;
        const auto testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
        ss << "/tmp/" << testInfo->test_suite_name() << "_" << testInfo->name() << ".im.shm"; 
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
 * @brief Ensure shm can be created with an initial frame.
 */
TEST_F(Suite, CreateInit)
{
    Dao::Shape shape{ 3,4,1 };
    int16_t frame[] = { 1,2,3,4,5,6,7,8,9,10,11,12 };
    Dao::Shm smem(shmPath_, frame, shape);

    IMAGE image {};
    ASSERT_EQ(daoShmShm2Img(shmPath_.c_str(), &image), DAO_SUCCESS);
    ASSERT_EQ(std::memcmp(frame, image.array.V, image.md->nelement * sizeof(int16_t)), 0);
    ASSERT_EQ(image.md->naxis, std::size(shape));
    ASSERT_EQ(image.md->atype, _DATATYPE_INT16);
    ASSERT_EQ(image.md->size[0], shape[0]);
    ASSERT_EQ(image.md->size[1], shape[1]);
    ASSERT_EQ(image.md->size[2], shape[2]);
    ASSERT_EQ(daoShmCloseShm(&image), DAO_SUCCESS);
}

/**
 * @brief Ensure shm can be created with uninitialised frame.
 */
TEST_F(Suite, CreateNoInit)
{
    Dao::Shape shape{ 3,4,1 };
    Dao::Shm<int16_t> smem(shmPath_, nullptr, shape);

    IMAGE image {};
    ASSERT_EQ(daoShmShm2Img(shmPath_.c_str(), &image), DAO_SUCCESS);
    ASSERT_EQ(image.md->naxis, std::size(shape));
    ASSERT_EQ(image.md->atype, _DATATYPE_INT16);
    ASSERT_EQ(image.md->size[0], shape[0]);
    ASSERT_EQ(image.md->size[1], shape[1]);
    ASSERT_EQ(image.md->size[2], shape[2]);
    ASSERT_EQ(daoShmCloseShm(&image), DAO_SUCCESS);
}

/**
 * @brief Ensure exception is thrown upon a creation error.
 */
TEST_F(Suite, CreateError)
{
    EXPECT_ANY_THROW(
        Dao::Shm<int16_t> smem(
            "",
            nullptr,
            { 3,4,1 }
        )
    );
}

/**
 * @brief Ensure shm opens.
 */
TEST_F(Suite, Open)
{
    IMAGE image {};
    uint32_t shape[] = { 1,1 };
    const auto createStatus = daoShmImageCreate(
        &image, shmPath_.c_str(),
        std::size(shape), shape,
        _DATATYPE_INT16, 1, 0
    );

    ASSERT_EQ(createStatus, DAO_SUCCESS);
    ASSERT_NO_THROW(Dao::Shm<int16_t> smem(shmPath_));
    ASSERT_EQ(daoShmCloseShm(&image), DAO_SUCCESS);
}

/**
 * @brief Ensure exception is thrown upon opening error.
 */
TEST_F(Suite, OpenError)
{
    EXPECT_ANY_THROW(Dao::Shm<int16_t> smem(""));
}

/**
 * @brief Ensure frame is written and then read correctly.
 */
TEST_F(Suite, ReadWrite)
{
    Dao::Shm<float> smem(shmPath_, nullptr, { 1,1 });

    float frame[] = { 3.1415f };
    smem.set_data(frame);

    float *frame_ = smem.get_data();
    ASSERT_EQ(std::memcmp(frame, frame_, smem.get_element_count() * sizeof(float)), 0);
}

/**
 * @brief Ensure correct frame sync via spinning.
 */
TEST_F(Suite, SpinSync)
{
    bool syncDone = false;
    float frame[] = { 3.1415f };
    Dao::Shm<float> smem(shmPath_, nullptr, { 1,1 });

    std::thread waiter ([&]() {
        float *frame_ = smem.get_data(Dao::ShmSync::SPIN);
        syncDone = true;
    });

    sleep(1);
    smem.set_data(frame);

    sleep(1);
    ASSERT_EQ(syncDone, true);
    waiter.join();
}

/**
 * @brief Ensure correct frame sync via spin w/ cnt0 specified.
 */
TEST_F(Suite, SpinSyncTimeout)
{
    bool syncDone = false;
    float frame[] = { 3.1415f };
    Dao::Shm<float> smem(shmPath_, nullptr, { 1,1 });

    std::thread waiter ([&]() {
        float *frame_ = smem.get_data(Dao::ShmSync::SPIN, 1);
        syncDone = true;
    });

    sleep(1);
    smem.set_data(frame);

    sleep(1);
    ASSERT_EQ(syncDone, true);
    waiter.join();
}

/**
 * @brief Ensure correct frame sync via semaphore.
 */
TEST_F(Suite, SemSync)
{
    bool syncDone = false;
    float frame[] = { 3.1415f };
    Dao::Shm<float> smem(shmPath_, nullptr, { 1,1 });

    std::thread waiter ([&]() {
        float *frame_ = smem.get_data(Dao::ShmSync::SEM);
        syncDone = true;
    });

    sleep(1);
    smem.set_data(frame);

    sleep(1);
    ASSERT_EQ(syncDone, true);
    waiter.join();
}

/**
 * @brief Ensure correct frame sync via semaphore w/ timeout specified.
 */
TEST_F(Suite, SemSyncTimeout)
{
    bool syncDone = false;
    float frame[] = { 3.1415f };
    Dao::Shm<float> smem(shmPath_, nullptr, { 1,1 });

    const size_t timeout = 1;
    std::thread waiter ([&]() {
        float *frame_ = smem.get_data(Dao::ShmSync::SEM, timeout);
        syncDone = true;
    });

    sleep(timeout + 2);
    ASSERT_EQ(syncDone, true);
    waiter.join();
}

/**
 * @brief Ensure correct frame sync via specific semaphore.
 */
TEST_F(Suite, SemXSync)
{
    bool syncDone = false;
    float frame[] = { 3.1415f };
    Dao::Shm<float> smem(shmPath_, nullptr, { 1,1 });

    std::thread waiter ([&]() {
        float *frame_ = smem.get_data(Dao::ShmSync::SEM7);
        syncDone = true;
    });

    sleep(1);
    smem.set_data(frame);

    sleep(1);
    ASSERT_EQ(syncDone, true);
    waiter.join();
}

/**
 * @brief Ensure shape is returned correctly.
 */
TEST_F(Suite, GetShape)
{
    Dao::Shape shape{ 1, 1 };
    Dao::Shm<float> smem(shmPath_, nullptr, shape);

    Dao::Shape shape_ = smem.get_shape();
    ASSERT_EQ(shape.size(), shape_.size());
    ASSERT_EQ(std::memcmp(shape.data(), shape_.data(), shape.size() * sizeof(uint32_t)), 0);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
