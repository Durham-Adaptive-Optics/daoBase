#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <iostream>
#include <daoLog.hpp>

/*
// TODO: Expand the testing
TEST(test_log_file, set_up) 
{
    std::string filepath = "/tmp/dao_log.txt";
    Dao::Log::Logger log(Dao::Log::Logger::DESTINATION::FILE, filepath);

    log.SetLevel(Dao::Log::LEVEL::TRACE);
    Dao::Log::LOG_MESSAGE message;
    message.comp_name = "test Comp";
    message.frame_number = 0;
    message.level = Dao::Log::LEVEL::TRACE;
    message.message = "Test Message";

    for(int i = 0; i < 10; i++)
    {
        message.frame_number++;
        log.log(message);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
*/
TEST(test_log_screen, set_up) 
{
    std::string name = "test Comp";
    Dao::Log::Logger log(name, Dao::Log::Logger::DESTINATION::SCREEN);

    log.SetLevel(Dao::Log::LEVEL::TRACE);
    for(int i = 0; i < 10; i++)
    {
        log.Trace("test - %i",1);
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}


// TEST(test_log_network, set_up) 
// {

//     std::string name = "test Comp";
//     std::string ip = "127.0.0.1";
//     int port = 5555;
//     Dao::Log::Logger log(name, Dao::Log::Logger::DESTINATION::NETWORK, ip, port);

//     log.SetLevel(Dao::Log::LEVEL::TRACE);
//     Dao::Log::LOG_MESSAGE message;
//     message.comp_name = name;
//     message.level = Dao::Log::LEVEL::TRACE;
//     message.message = "Test Message";

//     for(int i = 0; i < 10; i++)
//     {
//         log.log(message);
//         std::this_thread::sleep_for(std::chrono::microseconds(100));
//     }
//     std::this_thread::sleep_for(std::chrono::milliseconds(1000));
// }




int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}