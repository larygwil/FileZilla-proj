#include "platform.hpp"

#if WINDOWS
#include "windows.cpp"
#else
#include "unix.cpp"
#endif

