#include "statistics.hpp"

#include <iostream>

#ifdef USE_STATISTICS

statistics::type stats = {0};

void print_stats( unsigned long long start, unsigned long long stop )
{
	std::cerr << "Evaluated positions:    " << stats.evaluated_leaves << std::endl;
	std::cerr << "Intermediate positions: " << stats.evaluated_intermediate << std::endl;
#if USE_QUIESCENCE
	std::cerr << "Quiescent moves:        " << stats.quiescence_moves << std::endl;
#endif
	if( stats.evaluated_intermediate || stats.evaluated_intermediate ) {
		std::cerr << "Time per position:      " << ((stop - start) * 1000 * 1000) / (stats.evaluated_intermediate + stats.evaluated_intermediate) << " ns" << std::endl;
		if( stop != start ) {
			std::cerr << "Positions per second:   " << (1000 * (stats.evaluated_intermediate + stats.evaluated_intermediate)) / (stop - start) << std::endl;
		}
	}
}

void reset_stats()
{
	memset( &stats, 0, sizeof(statistics::type) );
}

#endif
