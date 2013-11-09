#include "platform.hpp"

#include <algorithm>
#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <iostream>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif

uint64_t timer_precision()
{
	return 1000000ull;
}

uint64_t get_time()
{
	timeval tv = {0, 0};
	gettimeofday( &tv, 0 );

	uint64_t ret = static_cast<uint64_t>(tv.tv_sec) * 1000 * 1000 + tv.tv_usec;
	return ret;
}

void console_init()
{
	setvbuf(stdout, NULL, _IONBF, 0);
}


unsigned int get_cpu_count()
{
	return std::min( long(MAX_THREADS), sysconf(_SC_NPROCESSORS_ONLN) );
}


int get_system_memory()
{
#ifndef __APPLE__
	uint64_t pages = sysconf(_SC_PHYS_PAGES);
	uint64_t page_size = sysconf(_SC_PAGE_SIZE);
	return pages * page_size / 1024 / 1024;
#else
	uint64_t v = 0;
	size_t l = sizeof(v);
	sysctlbyname("hw.memsize", &v, &l, 0, 0);
	return v / 1024 / 1024;
#endif
}


void* page_aligned_malloc( uint64_t size )
{
	uint64_t page_size = get_page_size();
	uint64_t alloc = page_size + size;
	if( size % page_size ) {
		alloc += page_size - size % page_size;
	}

	void* p = mmap( 0, alloc, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0 );

	if( p && p != MAP_FAILED ) {
		*reinterpret_cast<uint64_t*>(p) = alloc;
		return reinterpret_cast<char*>(p) + page_size;
	}
	else {
		std::cerr << "Memory allocation failed: " << errno << std::endl;
		return 0;
	}
}


void aligned_free( void* p )
{
	if( p ) {
		uint64_t page_size = get_page_size();
		p = reinterpret_cast<char*>(p) - page_size;
		uint64_t alloc = *reinterpret_cast<uint64_t*>(p);
		int res = munmap( p, alloc );
		if( res ) {
			std::cerr << "Deallocation failed: " << errno << std::endl;
		}
	}
}


uint64_t get_page_size()
{
	return static_cast<uint64_t>(getpagesize());
}

bool uses_native_popcnt()
{
	// We may lie
	return true;
}

bool cpu_has_popcnt()
{
	// We may lie
	return true;
}

void millisleep( int ms )
{
	timespec t;
	if( clock_gettime( CLOCK_REALTIME, &t ) != 0 ) {
		return;
	}

	t.tv_sec += ms / 1000;
	t.tv_nsec += (ms % 1000) * 1000000;
	if( t.tv_nsec >= 1000000000 ) {
		t.tv_nsec -= 1000000000;
		++t.tv_sec;
	}

	while( clock_nanosleep( CLOCK_REALTIME, TIMER_ABSTIME, &t, 0 ) == EINTR ) {
		// Repeat in case of a signal.
	}
}
