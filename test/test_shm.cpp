#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <iostream>


extern "C" {
#include <dao.h>
}


TEST(test_log_screen, set_up) 
{
    std::string name = "/tmp/test_build.im.shm";
    uint32_t size[2];
    size[0] = 10; 
    size[1] = 1;

    IMAGE* img = (IMAGE*)malloc(sizeof(IMAGE));
    const char * fp = name.c_str();
    long naxis = 2;
    uint8_t atype = 9;
    int shared = 1;
    int NBkw = 0;
    int_fast8_t a = daoShmImageCreate(img, fp, naxis, size, atype, shared, NBkw);

    EXPECT_TRUE(false);
}


int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}