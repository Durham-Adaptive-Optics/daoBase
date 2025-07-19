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
#include <daoComponentUpdateThread.hpp>
#include <daoLog.hpp>

namespace Dao
{
    class ComponentBase : public StateMachine
    {
        public:
            ComponentBase(std::string name, Dao::Log::Logger& logger, std::string ip, int port, int zmq_core=-1, int update_core=-1)
            : StateMachine(logger)
            , m_name(name)
            , m_ip(ip)
            , m_port(port)
            , m_log(logger)
            {
                // create templete message
                m_zmq_thread = std::make_unique<ComponentZmqThread>(m_name, logger, zmq_core, 0, false);
                m_update_thread = std::make_unique<ComponentUpdateThread>(m_name, logger, update_core, 0, false);
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
                m_zmq_thread->setCallback([this](std::string payload) {
                    this->PROCESS_OTHER(payload);
                });
                m_zmq_thread->Spawn();
                m_zmq_thread->Start();
            }

            void PostEnable()
            {
                // called during the enable command to launch the update thread.
                m_update_thread->Spawn();
                m_update_thread->Start(); 
            }           

            void PostDisable()
            {
                // TODO this maybe generalise and called in 
                if(m_update_thread->isRunning())
                {
                    m_update_thread->Stop();
                }

                if(m_update_thread->isSpawned())
                {
                    m_update_thread->Exit();
                    m_update_thread->Join();
                }
            }

            void Init(){};
            void Stop(){};
            void Enable(){};
            void Disable(){};
            void Run(){};
            void Idle(){};
            void OnFailure(){};
            void Recover(){};
            //virtual void Log(std::string message){};

            virtual void PROCESS_OTHER(std::string Payload)
            {
                m_log.Trace("PROCESS_OTHER(%s)", Payload.c_str());
            }

        protected:
            std::string m_name;
            std::string m_ip;
            int m_port;
            Log::Logger& m_log;


            std::unique_ptr<ComponentZmqThread> m_zmq_thread;
            std::unique_ptr<ComponentUpdateThread> m_update_thread;

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