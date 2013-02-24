#ifndef __UNIX_H__
#define __UNIX_H__

#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

#define PACKED(c, s) c s __attribute__((__packed__))
#define NONPACKED(s) s

uint64_t timer_precision();
uint64_t get_time();

void console_init();

unsigned int get_cpu_count();

// In MiB
int get_system_memory();

// Forward bitscan, returns zero-based index of lowest set bit.
// Precondition: mask != 0
inline uint64_t bitscan( uint64_t mask )
{
#if USE_BUILTINS
	return __builtin_ctzll( mask );
#else
	uint64_t index;
	asm \
	( \
	"bsfq %[mask], %[index]" \
	:[index] "=r" (index) \
	:[mask] "mr" (mask) \
	);

	return index;
#endif
}

// Forward bitscan, returns index of highest set bit.
// Precondition: mask != 0
inline uint64_t bitscan_reverse( uint64_t mask )
{
#if USE_BUILTINS
	return 63 - __builtin_clzll( mask );
#else
	uint64_t index;
	asm \
	( \
	"bsrq %[mask], %[index]" \
	:[index] "=r" (index) \
	:[mask] "mr" (mask) \
	);
	return index;
#endif
}

#if USE_GENERIC_POPCOUNT
#define popcount generic_popcount
#else
#define popcount __builtin_popcountll
#endif

#endif
