#include "platform.hpp"

#include <sys/time.h>
#include <stdio.h>
#include <errno.h>
#include <iostream>
#include <unistd.h>
#include <sys/mman.h>

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
	return sysconf(_SC_NPROCESSORS_ONLN);
}


int get_system_memory()
{
	uint64_t pages = sysconf(_SC_PHYS_PAGES);
	uint64_t page_size = sysconf(_SC_PAGE_SIZE);
	return pages * page_size / 1024 / 1024;
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
