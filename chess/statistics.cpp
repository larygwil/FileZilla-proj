#include "statistics.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"

#include <iostream>
#include <sstream>

#ifdef USE_STATISTICS

statistics::type stats;

void print_stats( uint64_t start, uint64_t stop )
{
	std::stringstream ss;

	ss << std::endl;
	ss << "Node stats:" << std::endl;
	ss << "  Total:            " << stats.full_width_nodes + stats.quiescence_nodes << std::endl;
	ss << "  Full-width:       " << stats.full_width_nodes << std::endl;
	ss << "  Quiescence:       " << stats.quiescence_nodes << std::endl;

	if( stats.full_width_nodes || stats.quiescence_nodes ) {
		ss << "  Time per node:    " << ((stop - start) * 1000 * 1000 * 1000) / (stats.full_width_nodes + stats.quiescence_nodes) / timer_precision() << " ns" << std::endl;
		if( stop != start ) {
			ss << "  Nodes per second: " << (timer_precision() * (stats.full_width_nodes + stats.quiescence_nodes) ) / (stop - start) << std::endl;
		}
	}

	ss << std::endl;
	ss << "Transposition table stats:" << std::endl;
	hash::stats s = transposition_table.get_stats( true );

	ss << "- Number of entries: " << s.entries << " (" << 100 * static_cast<double>(s.entries) / transposition_table.max_hash_entry_count() << "%)" << std::endl;
	ss << "- Lookup misses:     " << s.misses;
	if( s.misses + s.hits + s.best_move ) {
		ss << " (" << static_cast<double>(s.misses) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss << std::endl;
	ss << "- Lookup hits:       " << s.hits;
	if( s.misses + s.hits + s.best_move ) {
		ss << " (" << static_cast<double>(s.hits) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss << std::endl;
	ss << "- Lookup best moves: " << s.best_move;
	if( s.misses + s.hits + s.best_move ) {
		ss << " (" << static_cast<double>(s.best_move) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss << std::endl;
	ss << "- Index collisions:  " << s.index_collisions << std::endl;

	pawn_structure_hash_table::stats ps = pawn_hash_table.get_stats(true);

	ss << std::endl;
	ss << "Pawn structure hash table stats:" << std::endl;
	ss << "- Hits:     " << ps.hits;
	if( ps.hits + ps.misses ) {
		ss << " (" << 100 * static_cast<double>(ps.hits) / (ps.hits + ps.misses) << "%)";
	}
	ss << std::endl;
	ss << "- Misses:   " << ps.misses;
	if( ps.hits + ps.misses ) {
		ss << " (" << 100 * static_cast<double>(ps.misses) / (ps.hits + ps.misses) << "%)";
	}
	ss << std::endl << std::endl;

	std::cerr << ss.str();
}

void reset_stats()
{
	stats.full_width_nodes = 0;
	stats.quiescence_nodes = 0;
}

#endif
