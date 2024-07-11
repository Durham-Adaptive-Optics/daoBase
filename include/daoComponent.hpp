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
            Component(std::string name, Dao::Log::Logger& logger, std::string ip, int port, int core=-1)
            : ComponentBase(name, logger, ip, port, core)
            {
               ComponentBase::PostConstructor(static_cast<Dao::ComponentIfce*>(this));
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

        protected:

        private:
    };            
}; // namespace DAO

#endif /* DAO_COMPONENT_HPP */