#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#ifndef WINDOWS
  #if _WIN32 || _WIN64 || WIN32 || WIN64 || _MSC_VER
    #define WINDOWS 1
  #endif
#endif

#if WINDOWS
  #include "windows.hpp"
#else
  #include "unix.hpp"
#endif

/*
 * Allocates a block of memory of size bytes with a start adress being an
 * integer multiple of the system's memory page size.
 * Needs to be freed using aligned_free.
 */
void* page_aligned_malloc( uint64_t alignment );

void aligned_free( void* p );

// Returns the system's memory page size.
uint64_t get_page_size();

// Forward bitscan, returns zero-based index of lowest set bit and nulls said bit.
// Precondition: mask != 0
inline uint64_t bitscan_unset( uint64_t& mask ) {
	uint64_t index = bitscan( mask );
	mask &= mask - 1;
	return index;
}

#endif
