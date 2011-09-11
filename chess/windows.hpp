#ifndef __WINDOWS_H__
#define __WINDOWS_H__

#include <windows.h>
#include <intrin.h>

typedef unsigned long long uint64_t;

#define PACKED(s) \
	__pragma( pack( push, 1 ) )\
	s \
	__pragma( pack(pop) )

namespace std {
namespace tr1 {
}
using namespace tr1;
}

unsigned long long timer_precision();
unsigned long long get_time();
void console_init();

class mutex {
public:
	mutex();
	~mutex();

private:
	friend class scoped_lock;
	friend class condition;
	CRITICAL_SECTION cs_;
};


class scoped_lock
{
public:
	scoped_lock( mutex& m );
	~scoped_lock();

private:
	friend class condition;
	mutex& m_;
};


class condition
{
public:
	condition();
	~condition();

	void wait( scoped_lock& l );
	void wait( scoped_lock& l, unsigned long long timeout );
	void signal( scoped_lock& l );

private:
	CONDITION_VARIABLE cond_;
};


class thread {
public:
	thread();
	~thread();

	void spawn();
	void join();

	bool spawned();

	virtual void onRun() = 0;
private:

	HANDLE t_;
};

#endif

inline int bitscan( unsigned long long mask )
{
	unsigned long i;
	if( _BitScanForward64( &i, mask ) ) {
		++i;
	}
	else {
		i = 0;
	}

	return static_cast<int>(i);
}

int get_cpu_count();

// In MiB
int get_system_memory();

#define popcount __popcnt64