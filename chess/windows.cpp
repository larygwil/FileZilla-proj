#include "windows.hpp"

#include <windows.h>

#include <iostream>

namespace std {
using namespace tr1;
}


unsigned long long timer_precision()
{
	return 1000;
}


unsigned long long get_time() {
	return GetTickCount64();
}


void console_init()
{
	std::cout.setf(std::ios::unitbuf);
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
	locked_ = true;
}


scoped_lock::~scoped_lock()
{
	if( locked_ ) {
		LeaveCriticalSection( &m_.cs_ );
	}
}


void scoped_lock::lock()
{
	EnterCriticalSection( &m_.cs_ );
	locked_ = true;
}


void scoped_lock::unlock()
{
	LeaveCriticalSection( &m_.cs_ );
	locked_ = false;
}


condition::condition()
{
	InitializeConditionVariable( &cond_ );
}


condition::~condition()
{
}


void condition::wait( scoped_lock& l )
{
	SleepConditionVariableCS( &cond_, &l.m_.cs_, INFINITE );
}


void condition::wait( scoped_lock& l, unsigned long long timeout )
{
	timeout = timeout * 1000 / timer_precision();
	SleepConditionVariableCS( &cond_, &l.m_.cs_, static_cast<DWORD>(timeout) );
}


void condition::signal( scoped_lock& l )
{
	WakeConditionVariable( &cond_ );
}


thread::thread()
	: t_(0)
{
}


thread::~thread()
{
	join();
}


void thread::join()
{
	if( !t_ ) {
		return;
	}

	WaitForSingleObject( t_, INFINITE );

	CloseHandle( t_ );
	t_ = 0;
}


bool thread::spawned()
{
	return t_ != 0;
}


namespace {
extern "C" {
static DWORD run( void* p ) {
	reinterpret_cast<thread*>(p)->onRun();
	return 0;
}
}
}


void thread::spawn()
{
	join();

	t_ = CreateThread( 0, 0, &run, this, 0, 0 );
}

int get_cpu_count()
{
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = 0;
	DWORD len = 0;

	bool done = false;
	while( !GetLogicalProcessorInformation( buffer, &len ) ) {
		if( buffer ) {
			free(buffer);
		}

		if( GetLastError() != ERROR_INSUFFICIENT_BUFFER ) {
			return 1;
		}

		buffer = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(malloc( len ));
	}

	int count = 0;

	DWORD offset = 0;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = buffer;
	while( offset < len ) {
		switch (ptr->Relationship) {
			case RelationProcessorCore:
				count += static_cast<int>(popcount( ptr->ProcessorMask ));
				break;
			default:
				break;
		}
		offset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
		++ptr;
	}

	free( buffer );

	return count;
}

// In MiB
int get_system_memory()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(MEMORYSTATUSEX);

	GlobalMemoryStatusEx( &status );

	return static_cast<int>(status.ullTotalPhys / 1024 / 1024);
}
