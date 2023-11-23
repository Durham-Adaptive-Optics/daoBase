#include <daoLog2.hpp>

void 
Dao::Log::Logger::Debug(const char * fmt, ... )
{
    va_list args;
    va_start(args, fmt);
    log(Dao::Log::LEVEL::DEBUG, fmt, args);
    va_end(args);
}

void 
Dao::Log::Logger::Info(const char * fmt, ... )
{
    va_list args;
    va_start(args, fmt);
    log(Dao::Log::LEVEL::INFO, fmt, args);
    va_end(args);
}

void 
Dao::Log::Logger::Warning(const char * fmt, ... )
{
    va_list args;
    va_start(args, fmt);
    log(Dao::Log::LEVEL::WARNING, fmt, args);
    va_end(args);
}

void 
Dao::Log::Logger::Error(const char * fmt, ... )
{
    va_list args;
    va_start(args, fmt);
    log(Dao::Log::LEVEL::ERROR, fmt, args);
    va_end(args);
}

void 
Dao::Log::Logger::Critical(const char * fmt, ... )
{
    va_list args;
    va_start(args, fmt);
    log(Dao::Log::LEVEL::CRITICAL, fmt, args);
    va_end(args);
}

void 
Dao::Log::Logger::SetLevel(Dao::Log::LEVEL level)
{
    m_level = level;
}

Dao::Log::LEVEL 
Dao::Log::Logger::GetLevel()
{
    return m_level;
};


void
Dao::Log::Logger::log(Dao::Log::LEVEL level, const char * fmt, va_list args)
{
    char buf[4096]; // give a big buffer that should not be used
    vsnprintf( buf, 4095, fmt, args);

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    std::cout << "test_name" << ":" << std::put_time(&tm, "%y-%m-%d %OH:%OM:%OS") << " " << Dao::Log::LEVEL_TEXT.at(level) << " - " << buf << std::endl;
    
}
