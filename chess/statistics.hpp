#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#define USE_STATISTICS 1

#include "chess.hpp"
#include "time.hpp"

#ifdef USE_STATISTICS
class statistics {
public:
	statistics()
		: full_width_nodes()
		, quiescence_nodes()
		, total_full_width_nodes()
		, total_quiescence_nodes()
		, total_elapsed()
	{
	}

	void print( duration const& elapsed );
	void reset( bool total );
	void accumulate( duration const& elapsed );

	void print_total();

	uint64_t full_width_nodes;
	uint64_t quiescence_nodes;

	uint64_t total_full_width_nodes;
	uint64_t total_quiescence_nodes;

	duration total_elapsed;
};

extern statistics stats;

#endif

#endif
