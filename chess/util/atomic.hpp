#ifndef OCTOCHESS_ATOMIC_HEADER
#define OCTOCHESS_ATOMIC_HEADER

#if !DISABLE_ATOMICS
#include <atomic>

typedef std::atomic_ullong atomic_uint64_t;

inline void add_relaxed( atomic_uint64_t& a, uint64_t v )
{
	a.fetch_add( v, std::memory_order_relaxed );
}

inline void atomic_store( atomic_uint64_t& a, uint64_t v )
{
	a.store( v );
}

#else
// On some platforms, std::atomic is not available due to no kernel helper,
// e.g. Kindle and other ARM devices.
//
// If the platform only has a single core, we don't need actual atomics.

static_assert( MAX_THREADS == 1, "Error: Platform has no working std::atomic and uses more than 1 thread. This is not supported." );

typedef uint64_t atomic_uint64_t;

inline void add_relaxed( atomic_uint64_t& a, uint64_t v )
{
	a += v;
}

inline void atomic_store( atomic_uint64_t& a, uint64_t v )
{
	a = v;
}

#endif

#endif
