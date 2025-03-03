/**
 * @file    daoComponentIfce.hpp
 * @brief   componentIfce  definition
 *
 *
 * @author  D. Barr
 * @date    08 August 2022
 *
 * @bug No known bugs.
 *
 */
#ifndef DAO_COMPONENT_IFCE_HPP
#define DAO_COMPONENT_IFCE_HPP

#ifndef __cplusplus
#error This is a C++ include file and cannot be used from plain C
#endif

namespace Dao
{
    class ComponentIfce
    {
        public:
            virtual ~ComponentIfce(){};
            
            virtual void Init()     = 0;
            virtual void Stop()     = 0;
            virtual void Enable()   = 0;
            virtual void Disable()  = 0;
            virtual void Run()      = 0;
            virtual void Idle()     = 0;
            virtual void OnFailure()= 0;
            virtual void Recover()  = 0;
            virtual std::string GetStateText() = 0;
            
            std::string GetDiagnosticMessage() const { return m_diagnostic_msg; }

        private:
            std::string m_diagnostic_msg = "";
    };            
}; // namespace DAO

#endif /* DAO_COMPONENT_IFCE_HPP */