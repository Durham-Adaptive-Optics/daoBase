/**
 * @file    daoLogging.hpp
 * @brief   RT Log  definition
 *
 *
 * @author  D. Barr
 * @date    10 August 2022
 *
 * @bug No known bugs.
 *
 */
#ifndef DAO_LOG_HPP
#define DAO_LOG_HPP

#ifndef __cplusplus
#error This is a C++ include file and cannot be used from plain C
#endif


#include <mutex>
#include <thread>
#include <queue>
#include <memory>
#include <optional>
#include <algorithm>
#include <sstream>
#include <cstdarg>

#include <iostream>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <unistd.h>

#include <daoThreadSafeQueue.hpp>

// protobuf stuff
#include <daoLogging.pb.h>
#include <zmq.h>
#include <errno.h> 


static std::time_t time_now = std::time(nullptr);

namespace Dao
{
    // Log message stuct to hold the log message.
    namespace Log 
    {
        // Some useful enums for checking level
        enum class LEVEL : std::uint8_t {
//         enum LEVEL {
            TRACE       = 0, //(1U << 0),  /* 0b0000000000000001 */
            DEBUG       = 1, //(1U << 1),  /* 0b0000000000000010 */
            INFO        = 2, //(1U << 2),  /* 0b0000000000000100 */
            WARNING     = 3, //(1U << 3),  /* 0b0000000000001000 */
            ERROR       = 4, //(1U << 4),  /* 0b0000000000010000 */
            FATAL       = 5 // (1U << 5),  /* 0b0000000000100000 */
        };

        enum class EVENTS : uint8_t {
            NONE            = 0,
            UPDATE          = 1,
            STATE_CHANGE    = 2
        };

        // Map for level to string
        const std::map<LEVEL, std::string> LEVEL_TEXT = {
            {LEVEL::TRACE,          "[Trace] "},
            {LEVEL::DEBUG,          "[Debug] "},
            {LEVEL::INFO,           "[Info ] "},
            {LEVEL::WARNING,        "[WARNING]"},
            {LEVEL::ERROR,          "[Error] "},
            {LEVEL::FATAL,          "[Fatal] "}
        };

        class LOG_MESSAGE
        {
            public:
                std::string comp_name;
                std::string timestamp;
                std::string message;

                Dao::Log::LEVEL level;
                std::optional<Dao::Log::EVENTS> event;
        };

        class NetworkLog
        {
            public:
                NetworkLog(std::string ip = "127.0.0.1", int port = 5555, bool tcp = true)
                : m_ip(ip)
                , m_port(port)
                {
                    GOOGLE_PROTOBUF_VERIFY_VERSION;

                    std::cout << "Network Log: "<< ip << " " << port << std::endl;
                    std::cout << "Network Log: "<< m_ip << " " << m_port << std::endl;

                    char tmp[0x100];
                    if( gethostname(tmp, sizeof(tmp)) == 0 )
                    {
                        m_hostname = tmp;
                    }
                    // get stringstream of ip to connect.
                    m_connect.str("");
                    m_connect.clear();

                    if(tcp)
                    {
                        m_connect << "tcp://";
                    }
                    else
                    {
                        m_connect << "udp://";
                    }

                    m_connect << m_ip << ":";
                    m_connect << m_port;

                    std::cout << "Connecting to : " << m_connect.str() << std::endl;

                    try
                    {
                        m_context = zmq_ctx_new();
                        m_publisher = zmq_socket(m_context, ZMQ_PUB);
                        int rc = zmq_bind(m_publisher, m_connect.str().c_str());
                        assert(rc == 0 );
                    }
                    catch(const std::exception& e)
                    {
                        std::cerr << e.what() << '\n';
                    }
                }

                ~NetworkLog()
                {
                    zmq_close(m_publisher);
                    zmq_ctx_destroy(m_context);
                    google::protobuf::ShutdownProtobufLibrary();
                }

                void SendLog(LOG_MESSAGE& message)
                {   
                    std::string tmpString = construct_string(message);
                    tmpString = "This is a Message;";

                   std::cout << tmpString.length() << " : " << strlen(tmpString.c_str())  << std::endl;
                   std::cout << sizeof(tmpString.c_str()) << std::endl;
                   std::cout << tmpString << std::endl;
                   int rc = zmq_send (m_publisher, tmpString.c_str(), tmpString.length(), 0);
                    if(rc != 0)
                        dump_zmq_error(rc);
                }

                void dump_zmq_error(int error)
                {
                    switch (error)
                    {
                    case EAGAIN:
                        std::cout << error << ": " << "Non-blocking mode was requested and the message cannot be sent at the moment." << std::endl;
                        break;
                    case ENOTSUP:
                        std::cout << error << ": " << "The zmq_send() operation is not supported by this socket type." << std::endl;
                        break;
                    case EFSM:
                        std::cout << error << ": " << "The zmq_send() operation cannot be performed on this socket at the moment due to the socket not being in the appropriate state. This error may occur with socket types that switch between several states, such as ZMQ_REP. See the messaging patterns section of zmq_socket(3) for more information" << std::endl;
                        break;
                    case ETERM:
                        std::cout << error << ": " << "The ØMQ context associated with the specified socket was terminated." << std::endl;
                        break;
                    case ENOTSOCK:
                        std::cout << error << ": " << "The provided socket was invalid." << std::endl;
                        break;
                    case EINTR:
                        std::cout << error << ": " << "The operation was interrupted by delivery of a signal before the message was sent." << std::endl;
                        break;
                    case EFAULT:
                        std::cout << error << ": " << "Invalid message." << std::endl;
                        break;
                    default:
                        std::cout << error << ": " << "Unknow return." << std::endl;
                        break;
                    }
                    std::cout << error << ": " << zmq_strerror(error) << std::endl;
                    // assert(error == 0);
                }

            private:
                std::string construct_string(LOG_MESSAGE& message)
                {
                    std::string messageString;
                    Dao::Logs proto_message;
                    Dao::LogMessage * mess = proto_message.add_logs();   

                    // now copy from message to protoMessage.
                    mess->set_component_name(message.comp_name);
                    mess->set_level(m_level_text.at(message.level));
                    mess->set_log_message(message.message);
                    mess->set_time_stamp(message.timestamp);
                    mess->set_machine(m_hostname);

                    // get string 
                    proto_message.SerializeToString(&messageString);
                    return messageString;
                }

                const std::map<LEVEL, Dao::LogMessage::log_level> m_level_text = {
                    {LEVEL::TRACE,          Dao::LogMessage::TRACE},
                    {LEVEL::DEBUG,          Dao::LogMessage::DEBUG},
                    {LEVEL::INFO,           Dao::LogMessage::INFO},
                    {LEVEL::WARNING,        Dao::LogMessage::WARNING},
                    {LEVEL::ERROR,          Dao::LogMessage::ERROR},
                    {LEVEL::FATAL,          Dao::LogMessage::FATAL}
                };

                // network stuff
                std::string m_ip;
                int m_port;
                std::ostringstream m_connect;

                std::string m_hostname;
                
                // ZMQ stuff
                void * m_context;
                void * m_publisher;
        };

        // Entry point for the logs

        // contains the ability to log to screen, file, or network depending on configuration]
        // Spawns a thread that monitors a queue, and emptys the queue to destination
        class Logger
        {
            public:
                // This way multiple destinations can be used at once
                enum class DESTINATION : uint8_t {
                    NONE        = 0, // (1 << 0),  /* 0b0000000000000001 */
                    SCREEN      = 1, // (1 << 1),  /* 0b0000000000000010 */
                    FILE        = 2, // (1 << 2),  /* 0b0000000000000100 */
                    NETWORK     = 3, // (1 << 3),  /* 0b0000000000001000 */
                };

                Logger(std::string name, DESTINATION dst = DESTINATION::NONE, std::string filename_or_ip= "", int port = 0)
                : m_name(name)
                , m_level(LEVEL::INFO)
                , m_dst(dst)
                , m_alive(true)
                , m_queue()
                , m_timeout_ms(1)
                , m_filename_or_ip(filename_or_ip)
                , m_port(port)
                {
                    if(m_dst == DESTINATION::FILE)
                    {  
                        // open file
                        try
                        {
                            m_file.open(m_filename_or_ip, std::ofstream::out | std::ofstream::app);
                        }
                        catch(const std::exception& e)
                        {
                            std::cerr << e.what() << '\n';
                        }
                    }
                    else if (m_dst == DESTINATION::NETWORK)
                    {
                        // open network connection
                        // create network
                        m_network = new NetworkLog(m_filename_or_ip, port);
                    }
                    else if(m_dst == DESTINATION::SCREEN)
                    {

                    }
                    else
                    {
                    
                    }
                    m_log_thread = std::thread{&Logger::log_thread, this};
                }

                ~Logger()
                {
                    m_alive = false;
                    m_log_thread.join();

                    // clean up the file
                    if(m_file.is_open())
                        m_file.close();

                    // clean up the network
                    if(m_dst == DESTINATION::NETWORK)
                        delete m_network;

                }

                // only public interface are the log levels
                inline void Trace(const char * fmt, ... )
                {
                    
                    // std::cout <<  m_name << " Trace() " << std::endl;
                    // std::cout << "Requested: "  << " : " << Dao::Log::LEVEL_TEXT.at(Dao::Log::LEVEL::TRACE) << std::endl; 
                    // std::cout << "m_level " << " : [" << 0 << "] " << Dao::Log::LEVEL_TEXT.at(m_level) << std::endl;
                    // sleep(1);
                    if(Dao::Log::LEVEL::TRACE >= m_level)
                    {
                        va_list args;
                        va_start(args, fmt);
                        log(Dao::Log::LEVEL::TRACE, fmt, args);
                        va_end(args);
                    }
                }

                inline void Debug(const char * fmt, ... )
                {
                    if(Dao::Log::LEVEL::DEBUG >= m_level)
                    {
                        va_list args;
                        va_start(args, fmt);
                        log(Dao::Log::LEVEL::DEBUG, fmt, args);
                        va_end(args);
                    }
                }

                inline void Info(const char * fmt, ... )
                {
                    if(Dao::Log::LEVEL::INFO >= m_level)
                    {
                        va_list args;
                        va_start(args, fmt);
                        log(Dao::Log::LEVEL::INFO, fmt, args);
                        va_end(args);
                    }
                }

                inline void Warning(const char * fmt, ... )
                {
                    if(Dao::Log::LEVEL::WARNING >= m_level)
                    {
                        va_list args;
                        va_start(args, fmt);
                        log(Dao::Log::LEVEL::WARNING, fmt, args);
                        va_end(args);
                    }
                }

                inline void Error(const char * fmt, ... )
                {
                    if(Dao::Log::LEVEL::ERROR >= m_level)
                    {
                        va_list args;
                        va_start(args, fmt);
                        log(Dao::Log::LEVEL::ERROR, fmt, args);
                        va_end(args);
                    }
                }

                inline void Fatal(const char * fmt, ... )
                {
                    if(Dao::Log::LEVEL::FATAL >= m_level)
                    {
                        va_list args;
                        va_start(args, fmt);
                        log(Dao::Log::LEVEL::FATAL, fmt, args);
                        va_end(args);
                    }
                }


                // set LEVEL
                void SetLevel(LEVEL level){m_level = level;};
                LEVEL GetLevel(){return m_level;};

                DESTINATION GetDestination(){return m_dst;};

            protected:


            private:
                void log_thread()
                {
                    {
                        std::string tmp_name = "Log_" + m_name;
                        tmp_name.resize(15);
                        pthread_setname_np(pthread_self(), tmp_name.c_str() );
                    }
                    while(m_alive)
                    {
                        auto logItem = m_queue.pop();
                        if (logItem)
                        {
                            // this is network first as this is the perfromance option used mainly in operation to reduce number of 
                            // comparisions. 
                            // Then File
                            // Lastly Screen as if we are logging to screen we probably don't care for performance
                            if(m_dst == DESTINATION::NETWORK)
                            {
                                dump_network(logItem.value());
                            }
                            else if (m_dst == DESTINATION::FILE)
                            {
                                dump_file(logItem.value());
                            }
                            else if (m_dst == DESTINATION::SCREEN)
                            {
                                dump_screen(logItem.value());
                            }
                            else
                            {
                                // assume output none
                            }

                        }
                        else
                        {
                            // sleep for x timeout
                            std::this_thread::sleep_for(std::chrono::milliseconds(m_timeout_ms));
                        }
                    }
                    // try to empty queue (10 attempts then close)

                    
                    int attempt = 0;
                    while(true)
                    {   
                        auto logItem = m_queue.pop();
                        if(logItem)
                        {
                            if(m_dst == DESTINATION::NETWORK)
                            {
                                dump_network(logItem.value());
                            }
                            else if (m_dst == DESTINATION::FILE)
                            {
                                dump_file(logItem.value());
                            }
                            else if (m_dst == DESTINATION::SCREEN)
                            {
                                dump_screen(logItem.value());
                            }
                            attempt++;
                            if(attempt >= 10)
                                break;
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                inline void log(Dao::Log::LEVEL level, const char * fmt, va_list args)
                {
                    // my function for logging
                    char buf[4096]; // give a big buffer that should not be used
                    vsnprintf( buf, 4095, fmt, args);

                    LOG_MESSAGE message;
                    message.comp_name = m_name;
                    message.level = level;
                    // time_now = std::time(nullptr);

                    message.timestamp = getTimeString(std::time(nullptr));
                    message.message = buf;
                    m_queue.push(message);
                    // va_end called in previous function
                }


                inline std::string getTimeString(std::time_t time)
                {
                    std::stringstream timeString;
                    timeString << std::put_time(std::localtime(&time_now), "%y-%m-%d %OH:%OM:%OS");
                    return timeString.str();
                }

                std::string construct_string(LOG_MESSAGE& message)
                {
                    // don't use with network only local.
                    std::stringstream tmpString;
                    tmpString << message.comp_name << ":" << message.timestamp << " " << LEVEL_TEXT.at(message.level) << " - " << message.message;
                    return tmpString.str();       

                }

                void dump_file(LOG_MESSAGE& message)
                {
                    std::string tmpString = construct_string(message);
                    m_file << tmpString << "\n"; // std::
                }

                void dump_screen(LOG_MESSAGE& message)
                {
                    std::string tmpString = construct_string(message);
                    std::cout << tmpString << "\n"; // std::endl causes a flush and can take 1ms we remove this with thne /n
                }

                void dump_network(LOG_MESSAGE& message)
                {
                    m_network->SendLog(message);
                }

                std::string m_name;
                volatile bool m_alive;
                size_t m_timeout_ms;
                LEVEL m_level;
                DESTINATION m_dst;

                std::string m_filename_or_ip;
                int m_port;

                // file stuff
                std::ofstream m_file;

                // network stuff
                NetworkLog * m_network;
                

                // some sort of queue probably a boost or ring buffer depending on how we feel.
                // start with boost lock free queue move onto something more complex.
                ThreadSafeQueue<LOG_MESSAGE> m_queue;
                //ThreadSafeQueue<std::string> m_queue;
                std::thread m_log_thread;
        };    
    }; // namespace Log
}; // namespace DAO

#endif /* DAO_LOGGING_HPP */