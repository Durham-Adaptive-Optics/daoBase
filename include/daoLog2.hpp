/**
 * @file    daoLog2.hpp
 * @brief   RT Log to send to protobuf
 *
 *
 * @author  D. Barr
 * @date    10 August 2022
 *
 * @bug No known bugs.
 *
 */

#ifndef DAO_LOG2_HPP
#define DAO_LOG2_HPP

#ifndef __cplusplus
#error This is a C++ include file and cannot be used from plain C
#endif

#include <cstdarg>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <map>

namespace Dao
{
    namespace Log
    {
        enum class LEVEL : std::uint8_t {
//         enum LEVEL {
            NOSET       = 0,
            TRACE       = 5, //(1U << 0),  /* 0b0000000000000001 */
            DEBUG       = 10, //(1U << 1),  /* 0b0000000000000010 */
            INFO        = 20, //(1U << 2),  /* 0b0000000000000100 */
            WARNING     = 30, //(1U << 3),  /* 0b0000000000001000 */
            ERROR       = 40, //(1U << 4),  /* 0b0000000000010000 */
            CRITICAL    = 50 // (1U << 5),  /* 0b0000000000100000 */
        };
        const std::map<LEVEL, std::string> LEVEL_TEXT = {
            {LEVEL::TRACE,          "[TRACE]   "},
            {LEVEL::DEBUG,          "[DEBUG]   "},
            {LEVEL::INFO,           "[INFO ]   "},
            {LEVEL::WARNING,        "[WARNING] "},
            {LEVEL::ERROR,          "[ERROR]   "},
            {LEVEL::CRITICAL,       "[CRITICAL]"}
        };

        class Logger
        {
            public:
                Logger()
                : m_level(LEVEL::DEBUG)
                {

                }

                ~Logger()
                {

                }

                void Debug(const char * fmt, ... );
                void Info(const char * fmt, ... );
                void Warning(const char * fmt, ... );
                void Error(const char * fmt, ... );
                void Critical(const char * fmt, ... );

                void SetLevel(LEVEL level);
                LEVEL GetLevel();
                void log(Dao::Log::LEVEL level, const char * fmt, va_list args);

            private:
                // void log(Dao::Log::LEVEL level, const char * fmt, va_list args);

            
                LEVEL m_level;
        };
    }
}

#endif