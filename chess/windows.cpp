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
}


scoped_lock::~scoped_lock()
{
	LeaveCriticalSection( &m_.cs_ );
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
