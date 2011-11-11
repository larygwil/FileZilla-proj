#ifndef __WINDOWS_H__
#define __WINDOWS_H__

#include <windows.h>
#include <intrin.h>

typedef unsigned long long uint64_t;

#define PACKED(s) \
	__pragma( pack( push, 1 ) )\
	s \
	__pragma( pack(pop) )

#define NONPACKED(s) s

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

	void lock();
	void unlock();

private:
	friend class condition;
	mutex& m_;
	bool locked_;
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
	bool signalled_;
};


class thread {
public:
	thread();
	virtual ~thread();

	void spawn();
	void join();

	bool spawned();

	virtual void onRun() = 0;
private:

	HANDLE t_;
};

inline void bitscan( unsigned long long mask, unsigned long long& index )
{
	unsigned long i;
	_BitScanForward64( &i, mask );

	index = static_cast<unsigned long long >(i);
}

inline void bitscan_reverse( unsigned long long mask, unsigned long long& index )
{
	unsigned long i;
	_BitScanReverse64( &i, mask );

	index = static_cast<unsigned long long >(i);
}

int get_cpu_count();

// In MiB
int get_system_memory();

#define popcount __popcnt64

#define atoll _atoi64

#endif
