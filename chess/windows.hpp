#ifndef __WINDOWS_H__
#define __WINDOWS_H__

#define HAS_NATIVE_POPCOUNT 1

#if _WIN32_WINNT < 0x0600
  #undef _WIN32_WINNT
  #define _WIN32_WINNT 0x0600
#endif
#if WINVER < 0x0600
  #undef WINVER
  #define WINVER 0x0600
#endif

#include <windows.h>
#ifdef min
  #undef min
#endif
#ifdef max
  #undef max
#endif
#include <intrin.h>

typedef unsigned long long uint64_t;
typedef   signed long long  int64_t;

#define PACKED(c, s) \
	__pragma( pack( push, 1 ) )\
	c s \
	__pragma( pack(pop) )

#define NONPACKED(s) s

namespace std {
namespace tr1 {
}
using namespace tr1;
}

// Precision, not accuracy.
uint64_t timer_precision();

uint64_t get_time();
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
	void wait( scoped_lock& l, uint64_t timeout );
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

inline uint64_t bitscan( uint64_t mask )
{
#if __MINGW64__
	// We cannot use the __builtin_ffsll, as it is way more expensive:
	// It always adds 1 to the result, which we then have to subtract again.
	// Not even -O3 can save us there
	uint64_t index;
	asm \
	( \
	"bsfq %[mask], %[index]" \
	:[index] "=r" (index) \
	:[mask] "mr" (mask) \
	);

	return index;
#else
	unsigned long i;
	_BitScanForward64( &i, mask );

	return static_cast<uint64_t >(i);
#endif
}

inline uint64_t bitscan_reverse( uint64_t mask )
{
#if __MINGW64__
	uint64_t index;
	asm \
	( \
	"bsrq %[mask], %[index]" \
	:[index] "=r" (index) \
	:[mask] "mr" (mask) \
	);

	return index;
#else
	unsigned long i;
	_BitScanReverse64( &i, mask );

	return static_cast<uint64_t >(i);
#endif
}

unsigned int get_cpu_count();

// In MiB
int get_system_memory();

#if HAS_NATIVE_POPCOUNT

#if __MINGW64__
#define popcount __builtin_popcountll
#else
#define popcount __popcnt64
#endif

#else

inline uint64_t popcount( uint64_t w )
{
      w = w - ((w >> 1) & 0x5555555555555555ull);
      w = (w & 0x3333333333333333ull) + ((w >> 2) & 0x3333333333333333ull);
      w = (w + (w >> 4)) & 0x0f0f0f0f0f0f0f0full;
      return (w * 0x0101010101010101ull) >> 56;
}

#endif

#define atoll _atoi64

#endif
