#ifndef __STATISTICS_H__
#define __STATISTICS_H__

#include "chess.hpp"

#ifdef USE_STATISTICS
namespace statistics {
struct type {
	unsigned long long evaluated_leaves;
	unsigned long long evaluated_intermediate;
#if USE_QUIESCENCE
	unsigned long long quiescence_moves;
#endif
};
}

extern statistics::type stats;

void print_stats( unsigned long long start, unsigned long long stop );
void reset_stats( );

#endif

#endif
