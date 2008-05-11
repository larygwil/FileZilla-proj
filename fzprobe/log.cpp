#include "log.h"
#include "permissions.h"
#include <stdarg.h>

CLog log;

CLog::CLog()
{
  m_reopen = false;
  m_logfile = 0;
}

void CLog::Init()
{
  pthread_mutex_init(&m_mutex, 0);
}

bool CLog::Open()
{
  const char* filename = is_root() ? "/var/log/fzprobe.log" : "fzprobe.log";
  m_logfile = fopen(filename, "a");
  return m_logfile != 0;
}

void CLog::Reopen()
{
  m_reopen = true;
}

void CLog::Internal_Reopen()
{
  if (!m_reopen)
    return;

  const char* filename = is_root() ? "/var/log/fzprobe.log" : "fzprobe.log";
  FILE* file = fopen(filename, "a");
  if (file)
  {
     FILE* old = m_logfile;
    m_logfile = file;
    fclose(old);
  }
  m_reopen = false;
}

void CLog::Log(const char* fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  pthread_mutex_lock(&m_mutex);
  if (m_reopen)
    Internal_Reopen();
  vfprintf(m_logfile, fmt, ap);
  fflush(m_logfile);
  pthread_mutex_unlock(&m_mutex);
  va_end(ap);

  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}

void CLog::Logv(const char* fmt, va_list ap)
{
  va_list ap2;
  va_copy(ap2, ap);
  pthread_mutex_lock(&m_mutex);
  if (m_reopen)
    Internal_Reopen();
  vfprintf(m_logfile, fmt, ap);
  pthread_mutex_unlock(&m_mutex);

  vprintf(fmt, ap2);
  va_end(ap2);
}

