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


mutex::mutex()
{
	InitializeCriticalSection(&cs_);
}

mutex::~mutex()
{
	DeleteCriticalSection(&cs_);
}

scoped_lock::scoped_lock( mutex& m )
	: m_(m)
{
	EnterCriticalSection( &m.cs_ );
}

scoped_lock::~scoped_lock()
{
	LeaveCriticalSection( &m_.cs_ );
}