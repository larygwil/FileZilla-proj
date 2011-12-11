#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#if _MSC_VER
  #include "windows.hpp"
#else
  #include "unix.hpp"
#endif

/**
 * Allocates a block of memory of size bytes with a start adress being an
 * integer multiple of the system's memory page size.
 * Needs to be freed using aligned_free.
 */
void* page_aligned_malloc( unsigned long long alignment );

void aligned_free( void* p );

#endif
