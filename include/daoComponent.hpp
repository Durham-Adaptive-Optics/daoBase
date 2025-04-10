/**
 * @file    daoComponent.hpp
 * @brief   componentBase  definition
 *
 *
 * @author  D. Barr
 * @date    03 August 2022
 *
 * @bug No known bugs.
 *
 */
#ifndef DAO_COMPONENT_HPP
#define DAO_COMPONENT_HPP

#ifndef __cplusplus
#error This is a C++ include file and cannot be used from plain C
#endif

#include <daoComponentBase.hpp>
#include <daoComponentIfce.hpp>

#include <daoLog.hpp>

namespace Dao
{
    class Component : public ComponentBase, public ComponentIfce
    {
        public:
            /**
             * @brief Component constructor
             * @param name Unique name for this component
             * @param logger Logger instance for component logging
             * @param ip IP address for ZMQ communication
             * @param port Port number for ZMQ communication
             * @param autoRun If true, component will automatically advance to Running state
             * @param core CPU core to bind component threads to (-1 for no affinity)
             */
            Component(std::string name, Dao::Log::Logger& logger, std::string ip, int port, bool autoRun = false, int core=-1)
            : ComponentBase(name, logger, ip, port, core)
            , m_autoRun(autoRun)
            {
               ComponentBase::PostConstructor(static_cast<Dao::ComponentIfce*>(this));
               
               // If autoRun is enabled, advance the state machine to Running state
               if (m_autoRun) {
                   logger.Info("Auto-running component %s", name.c_str());
                   Init();
                   Enable();
                   Run();
               }
            }
               
            virtual ~Component()
            {

            }

            void Init() override
            {
                 postEvent(StateMachine::Events::Init);
            }

            void Stop() override
            {
                 postEvent(StateMachine::Events::Stop);
            }

            void Enable() override
            {
                 postEvent(StateMachine::Events::Enable);
                 ComponentBase::PostEnable();
            }

            void Disable() override
            {
                 postEvent(StateMachine::Events::Disable);
                 ComponentBase::PostDisable();
            }

            void Run() override
            {
                 postEvent(StateMachine::Events::Run);
            }

            void Idle() override
            {
                 postEvent(StateMachine::Events::Idle);
            }

            void OnFailure() override
            {
                 postEvent(StateMachine::Events::OnFailure);
            }

            void Recover() override
            {
                 postEvent(StateMachine::Events::Recover);
                 ComponentBase::PostDisable();
            }

            std::string GetStateText() override
            {
                 return currentState();
            }


            void PROCESS_OTHER(std::string payload) override
            {
                 // Process other messages
                 m_log.Info("Processing other message: %s", payload.c_str());
            }

        protected:

        private:
            bool m_autoRun; // Keeps track of whether component is in auto-run mode
    };            
}; // namespace DAO

#endif /* DAO_COMPONENT_HPP */