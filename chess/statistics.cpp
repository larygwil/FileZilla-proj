#include "statistics.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

#ifdef USE_STATISTICS

statistics stats;

void statistics::print( duration const& elapsed )
{
	std::stringstream ss;
	try {
		ss.imbue( std::locale("") );
	}
	catch( std::exception const& ) {
		// Who cares
	}

	ss << std::endl;
	ss << "Node stats:" << std::endl;
	ss << "  Total:            " << std::setw(11) << std::setfill(' ') << full_width_nodes + quiescence_nodes << std::endl;
	ss << "  Full-width:       " << std::setw(11) << std::setfill(' ') << full_width_nodes << std::endl;
	ss << "  Quiescence:       " << std::setw(11) << std::setfill(' ') << quiescence_nodes << std::endl;

	if( full_width_nodes || quiescence_nodes ) {
		if( !elapsed.empty() ) {
			ss << "  Nodes per second: " << std::setw(11) << std::setfill(' ') << elapsed.get_items_per_second(full_width_nodes + quiescence_nodes) << std::endl;
		}
		ss << "  Time per node:    " << std::setw(8) << elapsed.nanoseconds() / (full_width_nodes + quiescence_nodes) << " ns" << std::endl;
	}

	ss << std::endl;
	ss << "Transposition table stats:" << std::endl;
	hash::stats s = transposition_table.get_stats( true );

	ss << "- Number of entries: " << std::setw(11) << s.entries << " (" << 100 * static_cast<double>(s.entries) / transposition_table.max_hash_entry_count() << "%)" << std::endl;
	ss << "- Lookup misses:     " << std::setw(11) << s.misses;
	if( s.misses + s.hits + s.best_move ) {
		ss << " (" << static_cast<double>(s.misses) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss << std::endl;
	ss << "- Lookup hits:       " << std::setw(11) << s.hits;
	if( s.misses + s.hits + s.best_move ) {
		ss << " (" << static_cast<double>(s.hits) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss << std::endl;
	ss << "- Lookup best moves: " << std::setw(11) << s.best_move;
	if( s.misses + s.hits + s.best_move ) {
		ss << " (" << static_cast<double>(s.best_move) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss << std::endl;
	ss << "- Index collisions:  " << std::setw(11) << s.index_collisions << std::endl;

	pawn_structure_hash_table::stats ps = pawn_hash_table.get_stats(true);

	ss << std::endl;
	ss << "Pawn structure hash table stats:" << std::endl;
	ss << "- Hits:     " << std::setw(11) << ps.hits;
	if( ps.hits + ps.misses ) {
		ss << " (" << 100 * static_cast<double>(ps.hits) / (ps.hits + ps.misses) << "%)";
	}
	ss << std::endl;
	ss << "- Misses:   " << std::setw(11) << ps.misses;
	if( ps.hits + ps.misses ) {
		ss << " (" << 100 * static_cast<double>(ps.misses) / (ps.hits + ps.misses) << "%)";
	}
	ss << std::endl << std::endl;

	std::cerr << ss.str();
}

void statistics::print_total()
{
	std::stringstream ss;
	try {
		ss.imbue( std::locale("") );
	}
	catch( std::exception const& ) {
		// Who cares
	}

	ss << std::endl;
	ss << "Node stats:" << std::endl;
	ss << "  Total:            " << std::setw(14) << total_full_width_nodes + total_quiescence_nodes << std::endl;
	ss << "  Full-width:       " << std::setw(14) << total_full_width_nodes << std::endl;
	ss << "  Quiescence:       " << std::setw(14) << total_quiescence_nodes << std::endl;

	if( total_full_width_nodes || total_quiescence_nodes ) {
		if( !total_elapsed.empty() ) {
			ss << "  Nodes per second: " << std::setw(14) << std::setfill(' ') << total_elapsed.get_items_per_second(total_full_width_nodes + total_quiescence_nodes) << std::endl;
		}
		ss << "  Time per node:    " << std::setw(11) << total_elapsed.nanoseconds() / (total_full_width_nodes + total_quiescence_nodes) << " ns" << std::endl;
	}

	ss << std::endl;

	std::cerr << ss.str();
}


void statistics::accumulate( duration const& elapsed )
{
	total_full_width_nodes += full_width_nodes;
	total_quiescence_nodes += quiescence_nodes;
	total_elapsed += elapsed;
}


void statistics::reset( bool total )
{
	full_width_nodes = 0;
	quiescence_nodes = 0;
	if( total ) {
		total_full_width_nodes = 0;
		total_quiescence_nodes = 0;
		total_elapsed = duration();
	}
}

#endif
