#include "statistics.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

#ifdef USE_STATISTICS

statistics stats;

statistics::statistics()
	: quiescence_nodes()
	, total_full_width_nodes()
	, total_quiescence_nodes()
	, total_elapsed()
{
	for( int i = 0; i < MAX_DEPTH; ++i ) {
		full_width_nodes[i] = 0;
	}
}


void statistics::print( duration const& elapsed )
{
	std::stringstream ss;
	try {
		ss.imbue( std::locale("") );
	}
	catch( std::exception const& ) {
		// Who cares
	}

	uint64_t full = 0;
	uint64_t max_depth = 0;
	uint64_t busiest_count = 0;
	int busiest_depth = 0;
	for( int i = 0; i < MAX_DEPTH; ++i ) {
		if( full_width_nodes[i] ) {
			if( full_width_nodes[i] >= busiest_count ) {
				busiest_count = full_width_nodes[i];
				busiest_depth = i;
			}
			max_depth = i;
		}
		full += full_width_nodes[i];
	}

	ss << std::endl;
	ss << "Node stats:" << std::endl;
	ss << "  Total:             " << std::setw(11) << std::setfill(' ') << full + quiescence_nodes << std::endl;
	ss << "  Full-width:        " << std::setw(11) << std::setfill(' ') << full << std::endl;
	ss << "  Busiest/max depth: " << std::setw(6) << std::setfill(' ') << busiest_depth << " / " << std::setw(2) << max_depth << std::endl;
	ss << "  Quiescence:        " << std::setw(11) << std::setfill(' ') << quiescence_nodes << std::endl;

	if( full || quiescence_nodes ) {
		if( !elapsed.empty() ) {
			ss << "  Nodes per second:  " << std::setw(11) << std::setfill(' ') << elapsed.get_items_per_second(full + quiescence_nodes) << std::endl;
		}
		ss << "  Time per node:     " << std::setw(8) << elapsed.nanoseconds() / (full + quiescence_nodes) << " ns" << std::endl;
	}

	ss << std::endl;
	ss << "Transposition table stats:" << std::endl;
	hash::stats s = transposition_table.get_stats( true );

	ss << "- Number of entries: " << std::setw(11) << s.entries << " (";
	uint64_t max_hash_entry_count = transposition_table.max_hash_entry_count();
	if( max_hash_entry_count ) {
		ss << 100 * static_cast<double>(s.entries) / max_hash_entry_count << "%)";
	}
	ss << std::endl;

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
	for( int i = 0; i < MAX_DEPTH; ++i ) {
		total_full_width_nodes += full_width_nodes[i];
	}
	total_quiescence_nodes += quiescence_nodes;
	total_elapsed += elapsed;
}


void statistics::reset( bool total )
{
	for( int i = 0; i < MAX_DEPTH; ++i ) {
		full_width_nodes[i] = 0;
	}
	quiescence_nodes = 0;
	if( total ) {
		total_full_width_nodes = 0;
		total_quiescence_nodes = 0;
		total_elapsed = duration();
	}
}

void statistics::node( int ply )
{
	++full_width_nodes[ply];
}


uint64_t statistics::nodes()
{
	uint64_t full = 0;
	for( int i = 0; i < MAX_DEPTH; ++i ) {
		full += full_width_nodes[i];
	}

	return full;
}


int statistics::busiest_depth() const
{
	int busiest = 0;
	uint64_t busiest_count = 0;

	for( int i = 1; i < MAX_DEPTH && full_width_nodes[i]; ++i ) {
		if( full_width_nodes[i] >= busiest_count ) {
			busiest_count = full_width_nodes[i];
			busiest = i;
		}
	}
	return busiest;
}


int statistics::highest_depth() const
{
	int highest = 0;
	int i;
	for( i = 1; i < MAX_DEPTH && full_width_nodes[i]; ++i ) {
	}
	highest = i - 1;

	return highest;
}


void statistics::print_details() const
{
	std::stringstream ss;
	try {
		ss.imbue( std::locale("") );
	}
	catch( std::exception const& ) {
		// Who cares
	}

	for( int i = 1; i < MAX_DEPTH; ++i ) {
		uint64_t nodes = full_width_nodes[i];
		if( nodes ) {
			ss << std::setw(2) << std::setfill(' ') << i << " ";
			ss << std::setw(11) << nodes << std::endl;
		}
	}

	std::cerr << ss.str();
}


#endif
