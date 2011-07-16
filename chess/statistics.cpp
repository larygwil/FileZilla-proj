#include "statistics.hpp"
#include "hash.hpp"

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
	std::cerr << "Transposition table" << std::endl;
	std::cerr << "- Number of entries: " << stats.transposition_table_num_entries << std::endl;
	std::cerr << "- Fill level:        " << static_cast<double>(stats.transposition_table_num_entries) / max_hash_entry_count() << std::endl;
	std::cerr << "- Lookup hits:       " << stats.transposition_table_hits << std::endl;
	std::cerr << "- Lookup misses:     " << stats.transposition_table_misses << std::endl;
	std::cerr << "- Index collisions:  " << stats.transposition_table_collisions << std::endl;
}

void reset_stats()
{
	stats.evaluated_leaves = 0;
	stats.evaluated_intermediate = 0;
	stats.quiescence_moves = 0;
	stats.transposition_table_hits = 0;
	stats.transposition_table_misses = 0;
	stats.transposition_table_collisions = 0;
}

#endif
