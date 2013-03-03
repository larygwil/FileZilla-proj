#include "statistics.hpp"
#include "hash.hpp"
#include "util/logger.hpp"
#include "pawn_structure_hash_table.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

#if USE_STATISTICS

statistics::statistics()
	: quiescence_nodes()
	, total_full_width_nodes()
	, total_quiescence_nodes()
	, total_elapsed()
{
	try {
		ss_.imbue( std::locale("") );
	}
	catch( std::exception const& ) {
		// Who cares
	}
	for( int i = 0; i < MAX_DEPTH; ++i ) {
		full_width_nodes[i] = 0;
	}
}


void statistics::print( duration const& elapsed )
{
	ss_.str( std::string() );

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

	ss_ << std::endl;
	ss_ << "Node stats:" << std::endl;
	ss_ << "  Total:             " << std::setw(11) << std::setfill(' ') << full + quiescence_nodes << std::endl;
	ss_ << "  Full-width:        " << std::setw(11) << std::setfill(' ') << full << std::endl;
	ss_ << "  Busiest/max depth: " << std::setw(6) << std::setfill(' ') << busiest_depth << " / " << std::setw(2) << max_depth << std::endl;
	ss_ << "  Quiescence:        " << std::setw(11) << std::setfill(' ') << quiescence_nodes << std::endl;

	if( full || quiescence_nodes ) {
		if( !elapsed.empty() ) {
			ss_ << "  Nodes per second:  " << std::setw(11) << std::setfill(' ') << elapsed.get_items_per_second(full + quiescence_nodes) << std::endl;
		}
		ss_ << "  Time per node:     " << std::setw(8) << elapsed.nanoseconds() / (full + quiescence_nodes) << " ns" << std::endl;
	}

	ss_ << std::endl;
	ss_ << "Transposition table stats:" << std::endl;
	hash::stats s = transposition_table.get_stats( true );

	ss_ << "- Number of entries: " << std::setw(11) << s.entries << " (";
	uint64_t max_hash_entry_count = transposition_table.max_hash_entry_count();
	if( max_hash_entry_count ) {
		if( s.entries > max_hash_entry_count ) {
			s.entries = max_hash_entry_count;
		}
		ss_ << 100 * static_cast<double>(s.entries) / max_hash_entry_count << "%)";
	}
	ss_ << std::endl;

	ss_ << "- Lookup misses:     " << std::setw(11) << s.misses;
	if( s.misses + s.hits + s.best_move ) {
		ss_ << " (" << static_cast<double>(s.misses) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss_ << std::endl;
	ss_ << "- Lookup hits:       " << std::setw(11) << s.hits;
	if( s.misses + s.hits + s.best_move ) {
		ss_ << " (" << static_cast<double>(s.hits) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss_ << std::endl;
	ss_ << "- Lookup best moves: " << std::setw(11) << s.best_move;
	if( s.misses + s.hits + s.best_move ) {
		ss_ << " (" << static_cast<double>(s.best_move) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss_ << std::endl;
	ss_ << "- Index collisions:  " << std::setw(11) << s.index_collisions << std::endl;

	pawn_structure_hash_table::stats ps = pawn_hash_table.get_stats(true);

	ss_ << std::endl;
	ss_ << "Pawn structure hash table stats:" << std::endl;
	ss_ << "- Hits:     " << std::setw(11) << ps.hits;
	if( ps.hits + ps.misses ) {
		ss_ << " (" << 100 * static_cast<double>(ps.hits) / (ps.hits + ps.misses) << "%)";
	}
	ss_ << std::endl;
	ss_ << "- Misses:   " << std::setw(11) << ps.misses;
	if( ps.hits + ps.misses ) {
		ss_ << " (" << 100 * static_cast<double>(ps.misses) / (ps.hits + ps.misses) << "%)";
	}
	ss_ << std::endl << std::endl;

	dlog() << ss_.str();
}

void statistics::print_total()
{
	ss_.str( std::string() );

	ss_ << std::endl;
	ss_ << "Node stats:" << std::endl;
	ss_ << "  Total:            " << std::setw(14) << total_full_width_nodes + total_quiescence_nodes << std::endl;
	ss_ << "  Full-width:       " << std::setw(14) << total_full_width_nodes << std::endl;
	ss_ << "  Quiescence:       " << std::setw(14) << total_quiescence_nodes << std::endl;

	if( total_full_width_nodes || total_quiescence_nodes ) {
		if( !total_elapsed.empty() ) {
			ss_ << "  Nodes per second: " << std::setw(14) << std::setfill(' ') << total_elapsed.get_items_per_second(total_full_width_nodes + total_quiescence_nodes) << std::endl;
		}
		ss_ << "  Time per node:    " << std::setw(11) << total_elapsed.nanoseconds() / (total_full_width_nodes + total_quiescence_nodes) << " ns" << std::endl;
	}

	ss_ << std::endl;

	dlog() << ss_.str();
}


void statistics::accumulate( duration const& elapsed )
{
	for( int i = 0; i < MAX_DEPTH; ++i ) {
		total_full_width_nodes += full_width_nodes[i];
	}
	total_quiescence_nodes += quiescence_nodes;
	total_elapsed += elapsed;
}


void statistics::accumulate( statistics const& stats )
{
	for( int i = 0; i < MAX_DEPTH; ++i ) {
		full_width_nodes[i] += stats.full_width_nodes[i];
	}
	quiescence_nodes += stats.quiescence_nodes;
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


void statistics::print_details()
{
	ss_.str( std::string() );
	
	for( int i = 1; i < MAX_DEPTH; ++i ) {
		uint64_t nodes = full_width_nodes[i];
		if( nodes ) {
			ss_ << std::setw(2) << std::setfill(' ') << i << " ";
			ss_ << std::setw(11) << nodes << std::endl;
		}
	}

	dlog() << ss_.str();
}


#endif
