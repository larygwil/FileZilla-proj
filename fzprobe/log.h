#ifndef __LOG_H__
#define __LOG_H__

#include <pthread.h>
#include <stdio.h>

class CLog
{
public:
  CLog();

  void Init();
  bool Open();

  void Reopen();

  void Log(const char* fmt, ...);
  void Logv(const char* fmt, va_list ap);

protected:
  void Internal_Reopen();

  pthread_mutex_t m_mutex;

  bool m_reopen;

  FILE* m_logfile;
};

extern CLog log;

#endif //__LOG_H__

