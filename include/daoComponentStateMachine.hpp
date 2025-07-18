/**
 * @file    daoStateMachine.h
 * @brief   stateMachne  definition
 *
 *
 * @author  D. Barr
 * @date    12 Jul 2022
 *
 * @bug No known bugs.
 *
 */
#ifndef DAO_STATE_MACHINE_HPP
#define DAO_STATE_MACHINE_HPP

#ifndef __cplusplus
#error This is a C++ include file and cannot be used from plain C
#endif

#include <string>
#include <map>
#include <utility>
#include <functional>
#include <iostream>
#include <mutex>

#include <daoLog.hpp>


namespace Dao
{
    class StateMachine
    {
        public:
            StateMachine(Log::Logger& log)
            : m_state(State::Off)
            , m_log(log)
            {
                // launch thread
            }

            virtual ~StateMachine()
            {
            }

            enum class Events : unsigned {
                Init,
                Stop,
                Enable,
                Disable,
                Run,
                Idle,
                OnFailure,
                Recover
            };

            void postEvent(Events event)
            {
                m_log.Debug("post event: %s", m_event_text.at(event).c_str());
                // try and get mutex
                if(m_state_change_guard.try_lock())
                {

                    bool hasStateBeenChanged = false;
                    // iterate though all keys for ones matching event.
                    for (auto itr = m_event_state_transitions.begin(); itr != m_event_state_transitions.end(); itr++)
                    {   
                        if(event == (*itr).first)
                        {
                            // std::tuple<State, State, std::function<void()>> t = (*itr).second;
                            State requiredState         = std::get<0>((*itr).second);
                            State requestedState        = std::get<1>((*itr).second);
                            // std::function<void> &func    = std::get<2>((*itr).second);
                            if(requiredState == m_state)
                            {
                                // std::cout << "Required current state: " << m_state_text.at(requiredState) << " current state: " << m_state_text.at(m_state) << std::endl;
                                m_log.Debug("Required current state: %s  current state: %s", m_state_text.at(requiredState).c_str(), m_state_text.at(m_state).c_str());
                                // std::cout << "transitioning to state: " << m_state_text.at(requestedState) << std::endl;
                                m_log.Debug("Transitioning to state: %s",  m_state_text.at(requestedState).c_str());

                                changeState(event, requiredState, requestedState, std::get<2>((*itr).second));
                                hasStateBeenChanged = true;
                                break;
                            }
                        }
                    }

                    if(!hasStateBeenChanged)
                    {
                        m_log.Error("Could not change state.");
                    }
                    m_state_change_guard.unlock();
                }
                else
                {
                    m_log.Error("Unable to get mutex for event");
                }
            }

            std::string currentState(){return m_state_text.at(m_state);};

        protected:
        // entry and exit functions for states
            virtual void entry_Off()        {m_log.Trace("entry_Off()");};
            virtual void exit_Off()         {m_log.Trace("exit_Off()");}
            virtual void entry_Standby()    {m_log.Trace("entry_Standby()");};
            virtual void exit_Standby()     {m_log.Trace("exit_Standby()");};
            virtual void entry_Idle()       {m_log.Trace("entry_Idle()");};
            virtual void exit_Idle()        {m_log.Trace("exit_Idle()");};
            virtual void entry_Running()    {m_log.Trace("entry_Running()");};
            virtual void exit_Running()     {m_log.Trace("exit_Running()");};
            virtual void entry_Error()      {m_log.Trace("entry_Error()");};
            virtual void exit_Error()       {m_log.Trace("exit_Error()");};

        // supported state transitions
            virtual void transition_Off_Standby()   {m_log.Trace("transition_Off_Standby()");};
            virtual void transition_Standby_Off()   {m_log.Trace("transition_Standby_Off()");};
            virtual void transition_Standby_Idle()  {m_log.Trace("transition_Standby_Idle()");};
            virtual void transition_Idle_Standby()  {m_log.Trace("transition_Idle_Standby()");};
            virtual void transition_Idle_Running()  {m_log.Trace("transition_Idle_Running()");};
            virtual void transition_Running_Idle()  {m_log.Trace("transition_Running_Idle()");};
            virtual void transition_Idle_Error()    {m_log.Trace("transition_Idle_Error()");};
            virtual void transition_Running_Error() {m_log.Trace("transition_Running_Error()");};
            virtual void transition_Error_Idle()    {m_log.Trace("transition_Error_Idle()");};
        
        private:
            // Known states 
            enum class State : unsigned {
                None_,
                Error,
                Off,
                Standby,
                Idle,
                Running
            };

            //< States names
            const std::map<State, std::string> m_state_text = {
                {State::Error,          "Error"},
                {State::Off,            "Off"},
                {State::Standby,        "Standby"},
                {State::Idle,           "Idle"},
                {State::Running,        "Running"}
            };

            //< States names
            const std::map<Events, std::string> m_event_text = {
                {Events::Init,          "Init"},
                {Events::Stop,          "Stop"},
                {Events::Enable,        "Enable"},
                {Events::Disable,       "Disable"},
                {Events::Run,           "Run"},
                {Events::Idle,          "Idle"},
                {Events::OnFailure,     "OnFailure"},
                {Events::Recover,       "Recover"}
            };

            const std::map<State, std::function<void()>> m_state_entry_function = {
                {State::Error,      [&](){this->entry_Error();}},
                {State::Off,        [&](){this->entry_Off();}},
                {State::Standby,    [&](){this->entry_Standby();}},
                {State::Idle,       [&](){this->entry_Idle();}},
                {State::Running,    [&](){this->entry_Running();}},
            };

            const std::map<State, std::function<void()>> m_state_exit_function = {
                {State::Error,      [&](){this->exit_Error();}},
                {State::Off,        [&](){this->exit_Off();}},
                {State::Standby,    [&](){this->exit_Standby();}},
                {State::Idle,       [&](){this->exit_Idle();}},
                {State::Running,    [&](){this->exit_Running();}},
            };
            
            
            const std::multimap<Events, std::tuple<State,State,std::function<void()>>> m_event_state_transitions = {
            //   Event                                                                 startState      End State         Function for event
                {Events::Init,      std::make_tuple<State,State,std::function<void()>>(State::Off,     State::Standby,  [&](){this->transition_Off_Standby();}  )},
                {Events::Stop,      std::make_tuple<State,State,std::function<void()>>(State::Standby, State::Off,      [&](){this->transition_Standby_Off();}  )},
                {Events::Enable,    std::make_tuple<State,State,std::function<void()>>(State::Standby, State::Idle,     [&](){this->transition_Standby_Idle();} )},
                {Events::Disable,   std::make_tuple<State,State,std::function<void()>>(State::Idle,    State::Standby,  [&](){this->transition_Idle_Standby();} )},
                {Events::Run,       std::make_tuple<State,State,std::function<void()>>(State::Idle,    State::Running,  [&](){this->transition_Idle_Running();} )},
                {Events::Idle,      std::make_tuple<State,State,std::function<void()>>(State::Running, State::Idle,     [&](){this->transition_Running_Idle();} )},
                {Events::OnFailure, std::make_tuple<State,State,std::function<void()>>(State::Idle,    State::Error,    [&](){this->transition_Idle_Error();}   )},
                {Events::OnFailure, std::make_tuple<State,State,std::function<void()>>(State::Running, State::Error,    [&](){this->transition_Idle_Error();}   )},
                {Events::Recover,   std::make_tuple<State,State,std::function<void()>>(State::Error,   State::Idle,     [&](){this->transition_Error_Idle();}   )}
            };

            void changeState(Events event, State requiredStartState, State requestedEndState, std::function<void()> eventFunction)
            {
                try {
                    // calling exit function of current state
                    if(requiredStartState != requestedEndState) // check a state transition happens
                        m_state_exit_function.at(m_state)();

                    // calling transition
                    if(eventFunction)
                        eventFunction();

                    //calling entry function of new state
                    if(requiredStartState != requestedEndState) 
                        m_state_entry_function.at(requestedEndState)();

                    m_state = requestedEndState;
                }
                catch (const std::exception &e) {
                    m_log.Error("Failed to change state to %s: %s", m_state_text.at(requestedEndState), e.what());
                    m_state = State::Error;
                }
            }


            State m_state;
            std::mutex m_state_change_guard;
            Log::Logger& m_log;

    };
            
}; // namespace DAO

#endif