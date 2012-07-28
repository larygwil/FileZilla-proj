#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#define USE_STATISTICS 1

#include "chess.hpp"
#include "config.hpp"
#include "util/time.hpp"

#ifdef USE_STATISTICS
class statistics {
public:
	statistics();

	void node( int ply );

	uint64_t nodes();
	int highest_depth() const;
	int busiest_depth() const;

	void print( duration const& elapsed );
	void print_details() const;

	void reset( bool total );
	void accumulate( duration const& elapsed );

	void print_total();

	uint64_t quiescence_nodes;

	uint64_t total_full_width_nodes;
	uint64_t total_quiescence_nodes;

	duration total_elapsed;

private:
	uint64_t full_width_nodes[MAX_DEPTH];
};

extern statistics stats;

#endif

#endif
