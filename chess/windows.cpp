#include "windows.hpp"

#include <windows.h>

namespace std {
using namespace tr1;
}

unsigned long long get_time() {
	return GetTickCount64();
}

void console_init()
{
}
