#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <iostream>
#include <daoThreadSafeQueue.hpp>


// TODO: Expand the testing
TEST(test_queue, set_up) 
{

    float t = 1.0;
    Dao::ThreadSafeQueue<float> a;
    for(int i = 0; i < 10; i++)
        a.push(i*t);
    while(true)
    {
        auto g = a.pop();
        if(!g)
            break;
        printf("G: %f\n", g.value()); 
    }

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}