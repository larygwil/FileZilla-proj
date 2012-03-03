#include "platform.hpp"

#include <windows.h>

#include <iostream>

namespace std {
using namespace tr1;
}


uint64_t timer_precision()
{
	return 1000;
}


uint64_t get_time() {
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
	: signalled_()
{
	InitializeConditionVariable( &cond_ );
}


condition::~condition()
{
}


void condition::wait( scoped_lock& l )
{
	if( signalled_ ) {
		signalled_ = false;
		return;
	}
	SleepConditionVariableCS( &cond_, &l.m_.cs_, INFINITE );
	signalled_ = false;
}


void condition::wait( scoped_lock& l, uint64_t timeout )
{
	if( signalled_ ) {
		signalled_ = false;
		return;
	}
	timeout = timeout * 1000 / timer_precision();
	SleepConditionVariableCS( &cond_, &l.m_.cs_, static_cast<DWORD>(timeout) );
	signalled_ = false;
}


void condition::signal( scoped_lock& l )
{
	signalled_ = true;
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
static DWORD WINAPI run( void* p ) {
	reinterpret_cast<thread*>(p)->onRun();
	return 0;
}
}
}


void thread::spawn()
{
	join();

	t_ = CreateThread( NULL, 0, &run, this, 0, 0 );
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


void* page_aligned_malloc( uint64_t size )
{
	uint64_t page_size = get_page_size();

	uint64_t alloc = size;
	if( size % page_size ) {
		alloc += page_size - size % page_size;
	}

	void* p = VirtualAlloc( 0, alloc, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );

	if( p ) {
		return p;
	}
	else {
		DWORD err = GetLastError();
		std::cerr << "Memory allocation failed: " << err << std::endl;
		return 0;
	}
}


void aligned_free( void* p )
{
	if( p ) {
		BOOL res = VirtualFree( p, 0, MEM_RELEASE );
		if( !res ) {
			DWORD err = GetLastError();
			std::cerr << "Deallocation failed: " << err << std::endl;
		}
	}
}


uint64_t get_page_size()
{
	SYSTEM_INFO info = {0};
	GetSystemInfo( &info );
	return info.dwPageSize;
}
