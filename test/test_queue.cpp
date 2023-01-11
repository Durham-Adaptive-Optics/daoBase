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
        if (a.size() > 0)
        {
            auto g = a.pop();
            printf("G: %f\n", g); 
        }
        else
        {
            break;
        }  

    }

}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}