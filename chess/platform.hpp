#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#if _MSC_VER
  #include "windows.hpp"
#else
  #include "unix.hpp"
#endif

/*
 * Allocates a block of memory of size bytes with a start adress being an
 * integer multiple of the system's memory page size.
 * Needs to be freed using aligned_free.
 */
void* page_aligned_malloc( unsigned long long alignment );

void aligned_free( void* p );

// Returns the system's memory page size.
unsigned long long get_page_size();

// Forward bitscan, returns zero-based index of lowest set bit and nulls said bit.
// Precondition: mask != 0
inline unsigned long long bitscan_unset( unsigned long long& mask ) {
	unsigned long long index = bitscan( mask );
	mask &= mask - 1;
	return index;
}

#endif
