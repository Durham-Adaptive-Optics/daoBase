/**
 * @file    daoComponentZmqThread.h
 * @brief   zmq thread  definition
 *
 *
 * @author  D. Barr
 * @date    08 August 2022
 *
 * @bug No known bugs.
 *
 */
#ifndef DAO_COMPONENT_ZMQ_THREAD_HPP
#define DAO_COMPONENT_ZMQ_THREAD_HPP

#ifndef __cplusplus
#error This is a C++ include file and cannot be used from plain C
#endif

#include <string>
#include <memory>
#include <sstream>

#include <zmq.h>
#include <unistd.h>
#include <daoThread.hpp>
#include <daoComponentIfce.hpp>
#include <cerrno>
#include <daoCommand.pb.h>

namespace Dao
{
    class ComponentZmqThread : public Thread
    {
        public:
            ComponentZmqThread(std::string name, Log::Logger& logger, int core=-1, int handle=-1)
            : Thread("ZMQ_"+ name, logger, core, handle)
            , m_configured(false)
            , m_ip("")
            , m_port(0)
            , m_buffer_len(0)
            , m_error_code(0)
            , m_error_string("")
            , m_payload("")
            {
                GOOGLE_PROTOBUF_VERIFY_VERSION;
            }

            ~ComponentZmqThread()
            {
                zmq_close(m_responder);
                zmq_ctx_destroy(m_context);
                google::protobuf::ShutdownProtobufLibrary();
            }

            void Configure(std::string ip, int port, Dao::ComponentIfce * ifce, int core=-1, int thread_num=-1, size_t buffer_size=2147483647, size_t timeout_ms = 1000, bool tcp = true)
            {
                // here we configure the thread
                m_ip = ip;
                m_port = port;

                m_buffer_len = buffer_size;
                m_tcp = tcp;
                m_connect.str("");
                m_connect.clear();
                if(tcp)
                {
                    m_connect << "tcp://*:";
                }
                else
                {
                    m_connect << "udp://*:";
                }
                m_connect << m_port;
                m_timeout_ms = timeout_ms;

                m_ifce = ifce;

                if(core !=-1 && core > 0)
                {
                    m_core = core;
                }

                if(thread_num != 0)
                {
                    m_thread_number = thread_num;
                }

                m_configured = true;
            }

        protected:
            // these can be used to reset and reconfigure the thread.
            void OnceOnSpawn() override
            {
                int rc = 0;
                // here we create a few things
                // allocate large buffer:
                m_buf = malloc(m_buffer_len * sizeof(char));

                m_context   = zmq_ctx_new ();
                // int core = static_cast<int>(m_core);
                // rc = zmq_ctx_set(m_context, ZMQ_THREAD_AFFINITY_CPU_ADD, core);
                // assert(rc == 0);
                m_responder = zmq_socket (m_context, ZMQ_REP);
                int t = static_cast<int>(m_timeout_ms);
                rc = zmq_setsockopt(m_responder, ZMQ_RCVTIMEO, &t, sizeof(t));
                assert(rc == 0);
                // std::cout << m_connect.str().c_str() << std::endl;
                rc = zmq_bind(m_responder, m_connect.str().c_str());
                // std::cout << rc << std::endl;
                assert(rc == 0);
            };
 
            void OnceOnStart() override
            {
                // reset anything

            };
            
            void OnceOnStop() override
            {
                
            };

            void OnceOnExit() override
            {
                // clean up a few things
                free(m_buf);
            };

            void RestartableThread() override
            {
                if(m_configured)
                {
                    int nBytes = 0;
                    Dao::CommandMessage command;
                        // 
                    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
                    nBytes = zmq_recv(m_responder,m_buf, m_buffer_len*sizeof(char), 0);
                    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
                    if(nBytes == -1)
                    {
                        if(errno != EAGAIN)
                        {
                            // an error has occured do somethign
                            m_log.Error("%s recieved errno %d", m_thread_name.c_str(), errno);
                        }
                    } 
                    else if(nBytes > 0)
                    {
                        if(nBytes >= m_buffer_len)

                        {
                            m_log.Error("message too long... message truncated");
                        }
                        m_log.Debug("nBytes: %d");
                        // we have received something so process it.
                        // std::string *pstr = static_cast<std::string *>(buf);
                        std::string tmp;
                        tmp.resize(nBytes);
                        memcpy((char*)tmp.data(), m_buf, nBytes*sizeof(char));
                        command.ParseFromString(tmp);
                        process_message(command);
                    
                        // construct reply
                        std::string rep = construct_reply();
                        // reply with 
                        zmq_send(m_responder, rep.c_str(), rep.length(),0 );
                    }
                }

            }

            std::string m_ip;
            int m_port;
            std::ostringstream m_connect;

            size_t m_buffer_len;
            bool m_tcp;
            void * m_buf;

            size_t m_timeout_ms;
            
            Dao::ComponentIfce * m_ifce;
        private:

            void process_message(Dao::CommandMessage &command)
            {
                m_payload.str("");
                m_payload.clear();
                // process the received command

                std::cout << "Component: " << command.component().c_str() << std::endl;

                std::cout << "Function: ";
                switch(command.function())
                {
                case Dao::CommandMessage::EXEC:
                    std::cout << "EXEC " << std::endl;
                    process_EXEC(command.payload());
                    break;
                case Dao::CommandMessage::SETUP:
                    std::cout << "SETUP " << std::endl;
                    break;
                case Dao::CommandMessage::UPDATE:
                    std::cout << "UPDATE " << std::endl;
                    break;
                case Dao::CommandMessage::PING:
                    std::cout << "PING " << std::endl;
                    process_PING();
                    break;
                case Dao::CommandMessage::STATE:
                    std::cout << "STATE " << std::endl;
                    process_STATE();
                    break;
                case Dao::CommandMessage::SET_LOG_LEVEL:
                    std::cout << "SET_LOG_LEVEL " << std::endl;
                    process_SET_LOG_LEVEL(command.payload());
                    break;
                default:
                std::cout << "Unkown function " << std::endl;
                    break;
                }

            }

            void process_EXEC(std::string Payload)
            {
                m_log.Info("process_EXEC(%s)", Payload.c_str());
                std::cout << "Payload: "<< Payload << std::endl;
                if(Payload == "Init")
                {
                    // postEvent(StateMachine::Events::Init);
                    m_ifce->Init();
                }
                else if (Payload == "Stop")
                {
                    // postEvent(StateMachine::Events::Stop);
                    m_ifce->Stop();                }
                else if (Payload == "Enable")
                {
                    // postEvent(StateMachine::Events::Enable);
                    m_ifce->Enable();
                }
                else if (Payload == "Disable")
                {
                    // postEvent(StateMachine::Events::Disable);
                    m_ifce->Disable();
                }
                else if (Payload == "Run")
                {
                    // postEvent(StateMachine::Events::Run);
                    m_ifce->Run();
                }
                else if (Payload == "Idle")
                {
                    // postEvent(StateMachine::Events::Idle);
                    m_ifce->Idle();
                }
                else if (Payload == "OnFailue")
                {
                    // postEvent(StateMachine::Events::OnFailure);
                    m_ifce->OnFailure();
                }
                else if (Payload == "Recover")
                {
                    // postEvent(StateMachine::Events::Recover);
                    m_ifce->Recover();
                }
                else
                {
                    // unknown event;
                }
            }

            void process_PING()
            {
                m_log.Trace("Proces_PING()");
                // get pid of process and return 
                // std::string pid_string = ;
                m_payload << "PID: " << std::to_string(getpid());

            }

            void process_STATE()
            {
                m_log.Trace("Proces_STATE()");
                m_payload << "STATE: " << m_ifce->GetStateText();
            }

            void process_SET_LOG_LEVEL(std::string Payload)
            {
                m_log.Trace("Proces_SET_LOG_LEVEL(%s)", Payload.c_str());
                if(Payload == "TRACE")
                {
                    m_log.SetLevel(Log::LEVEL::TRACE);
                }
                else if(Payload == "DEBUG")
                {
                    m_log.SetLevel(Log::LEVEL::DEBUG);
                }
                else if(Payload == "INFO")
                {
                    m_log.SetLevel(Log::LEVEL::INFO);
                }
                else if(Payload == "WARNING")
                {
                    m_log.SetLevel(Log::LEVEL::WARNING);
                }
                else if(Payload == "ERROR")
                {
                    m_log.SetLevel(Log::LEVEL::ERROR);
                }
                else if(Payload == "CRITICAL")
                {
                    m_log.SetLevel(Log::LEVEL::CRITICAL);
                }
                else
                {
                    m_log.Warning("Unkown log level");
                }
            }

            std::string construct_reply()
            {

                std::string replyString;
                Dao::ReplyMessage my_reply;

                my_reply.set_status(Dao::ReplyMessage::SUCCESS);
                my_reply.set_payload(m_payload.str());
                if(m_error_code)
                {
                    my_reply.set_payload(m_error_string.str());
                }

                my_reply.SerializeToString(&replyString);
                return replyString;
            }

            // zero mq stuff
            void * m_context;
            void * m_responder;

            // return things for reply
            // these should be part of statemachine
            int m_error_code;
            std::ostringstream m_error_string;
            std::ostringstream m_payload;

           bool m_configured;

    };            
}; // namespace DAO

#endif /* DAO_COMPONENT_ZMQ_THREAD_HPP */