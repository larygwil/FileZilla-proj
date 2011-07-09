#include <windows.h>

unsigned long long get_time() {
  return GetTickCount64();
}

void console_init()
{
}
