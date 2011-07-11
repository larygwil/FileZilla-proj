#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#if _MSC_VER
  #include "windows.hpp"
#else
  #include "unix.hpp"
#endif

#endif
