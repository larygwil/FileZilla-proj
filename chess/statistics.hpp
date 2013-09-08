#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#ifndef USE_STATISTICS
#if DEVELOPMENT
#define USE_STATISTICS 2
#else
#define USE_STATISTICS 1
#endif
#endif

#include "config.hpp"
#include "util/time.hpp"
#include "util/atomic.hpp"
#include <sstream>

#if USE_STATISTICS
class context;
class statistics {
public:
	statistics();

	void node( int ply );

	uint64_t nodes();
	int highest_depth() const;
	int busiest_depth() const;

	void print( context& ctx, duration const& elapsed );
	void print_details();

	void reset( bool total );
	void accumulate( duration const& elapsed );
	void accumulate( statistics const& stats );

	void print_total();

	atomic_uint64_t quiescence_nodes;

	uint64_t total_full_width_nodes;
	uint64_t total_quiescence_nodes;

	duration total_elapsed;

private:
	atomic_uint64_t full_width_nodes[MAX_DEPTH];

	// Constructing a new stringstream and imbuing it with
	// a locale each time calling print is really espensive.
	// Re-use same stream.
	std::stringstream ss_;
};

#endif

#endif
