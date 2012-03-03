#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#define USE_STATISTICS 1

#include "chess.hpp"

#ifdef USE_STATISTICS
namespace statistics {
struct type {
	type()
		: full_width_nodes(), quiescence_nodes()
	{
	}

	uint64_t full_width_nodes;
	uint64_t quiescence_nodes;
};
}

extern statistics::type stats;

void print_stats( uint64_t start, uint64_t stop );
void reset_stats( );

#endif

#endif
