#include <stdlib.h>
#include <daoLog2_wrapper.hpp>
#include <daoLog2.hpp>

struct daoLog {
    void * obj;
};

daoLog_t * daoLog_create()
{
    daoLog_t * m;
    Dao::Log::Logger * obj;

    m      = (typeof(m))malloc(sizeof(*m));
    obj    = new Dao::Log::Logger();

    m->obj = obj;
    return m;
}


void daoLog_destroy(daoLog_t*m)
{
	if (m == NULL)
		return;
	delete static_cast<Dao::Log::Logger*>(m->obj);
	free(m);
}

void daoLogDebug(daoLog_t * m, const char* fmt, ...)
{
  Dao::Log::Logger * obj;
  if(m == NULL)
    return;

  obj = static_cast<Dao::Log::Logger *>(m->obj);
  if(Dao::Log::LEVEL::DEBUG >= obj->GetLevel())
  {
    va_list args;
    va_start(args, fmt);
    obj->log(Dao::Log::LEVEL::DEBUG, fmt, args);
    va_end(args);
  }
}


void daoLogInfo(daoLog_t * m, const char* fmt, ...)
{
  Dao::Log::Logger * obj;
  if(m == NULL)
    return;

  obj = static_cast<Dao::Log::Logger *>(m->obj);
  if(Dao::Log::LEVEL::INFO >= obj->GetLevel())
  {
    va_list args;
    va_start(args, fmt);
    obj->log(Dao::Log::LEVEL::INFO, fmt, args);
    va_end(args);
  }
}


void daoLogWarning(daoLog_t * m, const char* fmt, ...)
{
  Dao::Log::Logger * obj;
  if(m == NULL)
    return;

  obj = static_cast<Dao::Log::Logger *>(m->obj);
  if(Dao::Log::LEVEL::WARNING >= obj->GetLevel())
  {
    va_list args;
    va_start(args, fmt);
    obj->log(Dao::Log::LEVEL::WARNING, fmt, args);
    va_end(args);
  }
}


void daoLogError(daoLog_t * m, const char* fmt, ...)
{
  Dao::Log::Logger * obj;
  if(m == NULL)
    return;

  obj = static_cast<Dao::Log::Logger *>(m->obj);
  if(Dao::Log::LEVEL::ERROR >= obj->GetLevel())
  {
    va_list args;
    va_start(args, fmt);
    obj->log(Dao::Log::LEVEL::ERROR, fmt, args);
    va_end(args);
  }
}


void daoLogCritical(daoLog_t * m, const char* fmt, ...)
{
  Dao::Log::Logger * obj;
  if(m == NULL)
    return;

  obj = static_cast<Dao::Log::Logger *>(m->obj);
  if(Dao::Log::LEVEL::CRITICAL >= obj->GetLevel())
  {
    va_list args;
    va_start(args, fmt);
    obj->log(Dao::Log::LEVEL::CRITICAL, fmt, args);
    va_end(args);
  }
}