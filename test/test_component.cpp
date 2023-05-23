#include <gtest/gtest.h>
#include <string>

#include <daoComponent.hpp>

#include <thread>
#include <chrono>

// TODO: Expand the testing
TEST(compBaseCreation, set_up) {
    using namespace std::chrono_literals;
    std::string name = "Test";
    Dao::Log::Logger * logger = new Dao::Log::Logger(name, Dao::Log::Logger::DESTINATION::SCREEN);
    std::string ip = "localHost";
    int port = 5555;
    Dao::Component * a =  new Dao::Component(name, *logger, ip, port);
    std::this_thread::sleep_for(20ms);
    // do some delay
    delete a;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv); 
    return RUN_ALL_TESTS();
}