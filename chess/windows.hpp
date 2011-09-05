#ifndef __WINDOWS_H__
#define __WINDOWS_H__

#include <windows.h>

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

unsigned long long get_time();
void console_init();

class mutex {
public:
	mutex();
	~mutex();

private:
	friend class scoped_lock;
	CRITICAL_SECTION cs_;
};

class scoped_lock
{
public:
	scoped_lock( mutex& m );
	~scoped_lock();

private:
	mutex& m_;
};

#endif
