#include "statistics.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"

#include <iostream>

#ifdef USE_STATISTICS

statistics::type stats;

void print_stats( unsigned long long start, unsigned long long stop )
{
	std::cerr << std::endl;
	std::cerr << "Node stats:" << std::endl;
	  std::cerr << "  Total:            " << stats.full_width_nodes + stats.quiescence_nodes << std::endl;
	  std::cerr << "  Full-width:       " << stats.full_width_nodes << std::endl;
	  std::cerr << "  Quiescence:       " << stats.quiescence_nodes << std::endl;
	if( stats.full_width_nodes || stats.quiescence_nodes ) {
		std::cerr << "  Time per node:    " << ((stop - start) * 1000 * 1000 * 1000) / (stats.full_width_nodes + stats.quiescence_nodes) / timer_precision() << " ns" << std::endl;
		if( stop != start ) {
			std::cerr << "  Nodes per second: " << (timer_precision() * (stats.full_width_nodes + stats.quiescence_nodes) ) / (stop - start) << std::endl;
		}
	}

	std::cerr << std::endl;
	std::cerr << "Transposition table stats:" << std::endl;
	hash::stats s = transposition_table.get_stats( true );

	std::cerr << "- Number of entries: " << s.entries << std::endl;
	std::cerr << "- Fill level:        " << static_cast<double>(s.entries) / transposition_table.max_hash_entry_count() << std::endl;
	std::cerr << "- Lookup misses:     " << s.misses;
	if( s.misses + s.hits + s.best_move ) {
		std::cerr << " (" << static_cast<double>(s.misses) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	std::cerr << std::endl;
	std::cerr << "- Lookup hits:       " << s.hits;
	if( s.misses + s.hits + s.best_move ) {
		std::cerr << " (" << static_cast<double>(s.hits) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	std::cerr << std::endl;
	std::cerr << "- Lookup best moves: " << s.best_move;
	if( s.misses + s.hits + s.best_move ) {
		std::cerr << " (" << static_cast<double>(s.best_move) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	std::cerr << std::endl;
	std::cerr << "- Index collisions:  " << s.index_collisions << std::endl;

	pawn_structure_hash_table::stats ps = pawn_hash_table.get_stats(true);

	std::cerr << std::endl;
	std::cerr << "Pawn structure hash table stats:" << std::endl;
	std::cerr << "- Hits:     " << ps.hits << std::endl;
	std::cerr << "- Misses:   " << ps.misses << std::endl;
	std::cerr << "- Hit rate: " << static_cast<double>(ps.hits) / (ps.hits + ps.misses) << std::endl;

	std::cerr << std::endl;
}

void reset_stats()
{
	stats.full_width_nodes = 0;
	stats.quiescence_nodes = 0;
}

#endif
