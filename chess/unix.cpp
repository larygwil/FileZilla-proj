#include "unix.hpp"

#include <sys/time.h>
#include <stdio.h>

unsigned long long get_time()
{
	timeval tv = {0};
	gettimeofday( &tv, 0 );

	unsigned long long ret = static_cast<unsigned long long>(tv.tv_sec) * 1000 + (tv.tv_usec / 1000);
	return ret;
}

void console_init()
{
	setvbuf(stdout, NULL, _IONBF, 0);
}
