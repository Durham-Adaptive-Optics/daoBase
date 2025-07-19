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

#include <yaml-cpp/yaml.h>
#include <signal.h>
#include <unistd.h>

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
             * @param core CPU core to bind component threads to (-1 for no affinity)
             */
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


            void PROCESS_OTHER(std::string payload) override
            {
                 // Process other messages
                 m_log.Info("Processing other message: %s", payload.c_str());
            }

        protected:

        private:
    };

    /**
     * @brief Simple runs a blocking loop that waits for signals and handles them
     * @param m_log Logger reference for logging
     */
    void RunComponentLoop(Dao::Log::Logger& m_log);

    /**
     * @brief Generic factory function to create and optionally auto-run any Component-derived class
     * @tparam ComponentType The specific component class (must inherit from Component)
     * @param config_file Path to YAML configuration file
     * @return nothing when the component is closed via signal
     */
    template<typename ComponentType>
    void
    CreateComponentAndRun(const std::string& config_file)
    {
        // Load configuration
        if (config_file.empty()) {
            throw std::runtime_error("Configuration file path is required");
        }
        
        YAML::Node config = YAML::LoadFile(config_file);
        
        // Extract basic configuration
        const std::string name = config["static"]["name"].as<std::string>();
        const std::string ip = config["Control"]["ip"].as<std::string>();
        const int port = config["Control"]["port"].as<int>();
        
        // Setup logger
        Dao::Log::Logger* logger = nullptr;
        const std::string log_policy = config["Logging"]["policy"].as<std::string>();
        
        if (log_policy.compare("NETWORK") == 0) {
            const int log_port = config["Logging"]["port"].as<int>();
            const std::string log_ip = config["Logging"]["ip"].as<std::string>();
            logger = new Dao::Log::Logger(name, Dao::Log::Logger::DESTINATION::NETWORK, log_ip, log_port);
        } else if (log_policy == "FILE") {
            const std::string log_filename = config["Logging"]["filename"].as<std::string>();
            logger = new Dao::Log::Logger(name, Dao::Log::Logger::DESTINATION::FILE, log_filename);
        } else {
            logger = new Dao::Log::Logger(name, Dao::Log::Logger::DESTINATION::SCREEN);
        }

        bool autoRun = false;
        if (config["Control"]["autoRun"])
        {
            autoRun = config["Control"]["autoRun"].as<bool>();
        }
        
        logger->SetLevel(Dao::Log::LEVEL::TRACE);
        
        // Create component instance
        ComponentType* component = new ComponentType(name, *logger, ip, port, config);
        
        // Auto-run if requested
        if (autoRun)
        {
            component->Init();
            usleep(10000);  // Sleep for 100ms
            component->Enable();
            usleep(10000); // Sleep for 100ms
            component->Run();
        }

        RunComponentLoop(*logger);

        delete component;
        delete logger;
        
        return;
    }

}; // namespace Dao

#endif /* DAO_COMPONENT_HPP */