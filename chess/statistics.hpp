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

	unsigned long long full_width_nodes;
	unsigned long long quiescence_nodes;
};
}

extern statistics::type stats;

void print_stats( unsigned long long start, unsigned long long stop );
void reset_stats( );

#endif

#endif
