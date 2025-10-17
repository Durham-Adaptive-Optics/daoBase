/**
 * @ Author: Thomas N. Davies
 * @ Company: Centre for Advanced Instrumentation, Durham University
 * @ Contact: thomas.n.davies@durham.ac.uk
 * @ Create Time: 2025-10-17 14:47:41
 * @ Description: Tests for the modern dao shared memory C++ interface.
 */

#include <gtest/gtest.h>
#include <daoShm.hpp>

/* TESTS
    [x] Create shm w/ frame
    [x] Create shm wo/ frame
    [x] Create shm error
    [x] Open shm
    [x] Open shm error
    [x] Write frame
    [x] Get frame - now
    [-] Get frame - spin
    [-] Get frame - spin w/ target
    [-] Get frame - semaphore (SEM, SEMx)
    [-] Get frame - semaphore (SEM, SEMx) w/ timeout
*/

/**
 * @brief Ensure shm can be created with an initial frame.
 */
TEST(Creation, frame) {
    // create shm
    uint32_t shape[] = {3,4,1};
    int16_t frame[] = {1,2,3,4,5,6,7,8,9,10,11,12};
    Dao::Shm smem("/tmp/creation_frame.im.shm", frame, std::size(shape), shape);

    IMAGE image;
    ASSERT_EQ(daoShmShm2Img("/tmp/creation_frame.im.shm", &image), DAO_SUCCESS);
    ASSERT_EQ(image.md->naxis, std::size(shape));
    ASSERT_EQ(image.md->size[0], shape[0]);
    ASSERT_EQ(image.md->size[1], shape[1]);
    ASSERT_EQ(image.md->size[2], shape[2]);
    ASSERT_EQ(image.md->atype, _DATATYPE_INT16);

    for(size_t i = 0; i < image.md->nelement; ++i) {
        ASSERT_EQ(frame[i], image.array.SI16[i]);
    }
}

/**
 * @brief Ensure shm can be created with no frame.
 */
TEST(Creation, noFrame) {
    // create shm
    uint32_t shape[] = {3,4,1};
    Dao::Shm<int16_t> smem("/tmp/creation_noFrame.im.shm", nullptr, std::size(shape), shape);

    IMAGE image;
    ASSERT_EQ(daoShmShm2Img("/tmp/creation_noFrame.im.shm", &image), DAO_SUCCESS);
    ASSERT_EQ(image.md->naxis, std::size(shape));
    ASSERT_EQ(image.md->size[0], shape[0]);
    ASSERT_EQ(image.md->size[1], shape[1]);
    ASSERT_EQ(image.md->size[2], shape[2]);
    ASSERT_EQ(image.md->atype, _DATATYPE_INT16);
}

/**
 * @brief Ensure shm creation error throws.
 */
TEST(Creation, internalError) {
    // create shm
    uint32_t shape[] = {3,4,1};
    EXPECT_ANY_THROW(
         Dao::Shm<int16_t> smem(
            "",
            nullptr, 
            std::size(shape), 
            shape
        )
    );
}

/**
 * @brief Ensure shm opens.
 */
TEST(Open, Open) {
    // create shm
    IMAGE image;
    uint32_t shape[] = {1,1};
    const auto createStatus = daoShmImageCreate(
                        &image,
                        "/tmp/open_open.im.shm",
                        std::size(shape),
                        shape,
                        _DATATYPE_INT16,
                        1, // shared memory
                        0 // no keywords
                    ); 
    ASSERT_EQ(createStatus, DAO_SUCCESS);

    // load via interface
    ASSERT_ANY_THROW(Dao::Shm<int16_t> smem("/tmp/open_open.im.shm"));
}

/**
 * @brief Ensure shm open error throws.
 */
TEST(Open, internalError) {
    // create shm
    uint32_t shape[] = {3,4,1};
    EXPECT_ANY_THROW(Dao::Shm<int16_t> smem(""));
}

/**
 * @brief Ensure frame is written and then read correctly.
 */
TEST(IO, GetSet) {
    // create shm
    uint32_t shape[] = {1,1};
    Dao::Shm<float> smem("/tmp/io_getset.im.shm", nullptr, std::size(shape), shape);

    // write frame
    float frame[] = {3.1415f};
    smem.set_data(frame);

    // validate
    float *frame_ = smem.get_data();
    for(size_t i = 0; i < smem.get_element_count(); ++i) {
        ASSERT_EQ(frame[i], frame_[i]);
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
