/**
 * @file    daoComponentBase.h
 * @brief   componentBase  definition
 *
 *
 * @author  D. Barr
 * @date    03 August 2022
 *
 * @bug No known bugs.
 *
 */
#ifndef DAO_COMPONENT_BASE_HPP
#define DAO_COMPONENT_BASE_HPP

#ifndef __cplusplus
#error This is a C++ include file and cannot be used from plain C
#endif

#include <string>
#include <memory>
#include <sstream>

#include <unistd.h>
#include <daoThread.hpp>
#include <daoComponentStateMachine.hpp>
#include <daoComponentZmqThread.hpp>
#include <daoLog.hpp>

namespace Dao
{
    class ComponentBase : public StateMachine
    {
        public:
            ComponentBase(std::string name, Dao::Log::Logger& logger, std::string ip, int port)
            : StateMachine(logger)
            , m_name(name)
            , m_ip(ip)
            , m_port(port)
            , m_log(logger)
            {
                // create templete message
                m_zmq_thread = std::make_unique<ComponentZmqThread>(m_name, logger, 0, 0);
            }

            virtual ~ComponentBase()
            {
                m_zmq_thread->Stop();
                m_zmq_thread->Exit();
                m_zmq_thread->Join();
            }

            void PostConstructor(Dao::ComponentIfce* ifce)
            {
                m_zmq_thread->Configure(m_ip, m_port, ifce);
                m_zmq_thread->Spawn();
                m_zmq_thread->Start();
            }            

            void Init(){};
            void Stop(){};
            void Enable(){};
            void Disable(){};
            void Run(){};
            void Idle(){};
            void OnFailure(){};
            void Recover(){};
            void Log(std::string message){};

        protected:
            std::string m_name;
            std::string m_ip;
            int m_port;
            Log::Logger& m_log;


            std::unique_ptr<ComponentZmqThread> m_zmq_thread;

            class Context;

        private:
    };

    class ComponentBase::Context
    {
        struct RealTimeDataStruct;
        struct SharedMemoryDataStruct;
        struct DynamicDataStruct;
        struct StaticConfigStruct;
    };
}; // namespace DAO

#endif