#include "statistics.hpp"

#include "context.hpp"
#include "hash.hpp"
#include "util/logger.hpp"
#include "pawn_structure_hash_table.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

#if USE_STATISTICS

#if USE_STATISTICS >= 2
atomic_uint64_t statistics::full_eval_ = atomic_uint64_t();
atomic_uint64_t statistics::endgame_eval_ = atomic_uint64_t();
#endif

statistics::statistics()
	: quiescence_nodes()
	, total_full_width_nodes()
	, total_quiescence_nodes()
	, total_elapsed()
#if USE_STATISTICS >= 2
	, cutoffs_()
	, processed_()
#endif
	, full_width_nodes()
{
	try {
		ss_.imbue( std::locale("") );
	}
	catch( std::exception const& ) {
		// Who cares
	}
}


void statistics::print( context& ctx, duration const& elapsed )
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

	ss_ << "\n";
	ss_ << "Node stats:\n";
	ss_ << "  Total:             " << std::setw(11) << std::setfill(' ') << full + quiescence_nodes << "\n";
	ss_ << "  Full-width:        " << std::setw(11) << std::setfill(' ') << full << "\n";
	ss_ << "  Busiest/max depth: " << std::setw(6) << std::setfill(' ') << busiest_depth << " / " << std::setw(2) << max_depth << "\n";
	ss_ << "  Quiescence:        " << std::setw(11) << std::setfill(' ') << quiescence_nodes << "\n";

	if( full || quiescence_nodes ) {
		if( !elapsed.empty() ) {
			ss_ << "  Nodes per second:  " << std::setw(11) << std::setfill(' ') << elapsed.get_items_per_second(full + quiescence_nodes) << "\n";
		}
		ss_ << "  Time per node:     " << std::setw(8) << elapsed.nanoseconds() / (full + quiescence_nodes) << " ns\n";
	}

	ss_ << "\n";

#if USE_STATISTICS >= 2
	ss_ << "Transposition table stats:\n";
	hash::stats s = ctx.tt_.get_stats( true );

	ss_ << "- Number of entries: " << std::setw(11) << s.entries;
	uint64_t max_hash_entry_count = ctx.tt_.max_hash_entry_count();
	if( max_hash_entry_count ) {
		if( s.entries > max_hash_entry_count ) {
			s.entries = max_hash_entry_count;
		}
		ss_ << " (" << 100 * static_cast<double>(s.entries) / max_hash_entry_count << "%)";
	}
	ss_ << "\n";

	ss_ << "- Lookup misses:     " << std::setw(11) << s.misses;
	if( s.misses + s.hits + s.best_move ) {
		ss_ << " (" << static_cast<double>(s.misses) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss_ << "\n";
	ss_ << "- Lookup hits:       " << std::setw(11) << s.hits;
	if( s.misses + s.hits + s.best_move ) {
		ss_ << " (" << static_cast<double>(s.hits) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss_ << "\n";
	ss_ << "- Lookup best moves: " << std::setw(11) << s.best_move;
	if( s.misses + s.hits + s.best_move ) {
		ss_ << " (" << static_cast<double>(s.best_move) / (s.misses + s.hits + s.best_move) * 100 << "%)";
	}
	ss_ << "\n";
	ss_ << "- Index collisions:  " << std::setw(11) << s.index_collisions << "\n";

	pawn_structure_hash_table::stats ps = ctx.pawn_tt_.get_stats(true);

	ss_ << "\n";
	ss_ << "Pawn structure hash table stats:\n";
	ss_ << "- Entries:    " << std::setw(11) << ps.fill;
	uint64_t max_pawn_hash_entry_count = ctx.tt_.max_hash_entry_count();
	if( max_pawn_hash_entry_count ) {
		if( ps.fill > max_hash_entry_count ) {
			ps.fill = max_hash_entry_count;
		}
		ss_ << " (" << 100 * static_cast<double>(ps.fill) / max_pawn_hash_entry_count << "%)";
	}
	ss_ << "\n";
	ss_ << "- Hits:       " << std::setw(11) << ps.hits;
	if( ps.hits + ps.misses ) {
		ss_ << " (" << 100 * static_cast<double>(ps.hits) / (ps.hits + ps.misses) << "%)";
	}
	ss_ << "\n";
	ss_ << "- Misses:     " << std::setw(11) << ps.misses;
	if( ps.hits + ps.misses ) {
		ss_ << " (" << 100 * static_cast<double>(ps.misses) / (ps.hits + ps.misses) << "%)";
	}
	ss_ << "\n- Collisions: " << std::setw(11) << ps.collision << "\n\n";

	if( cutoffs_ > 0 ) {
		ss_ << "Moves per cutoff: " << static_cast<double>(processed_) / cutoffs_ << "\n\n";
	}

	ss_ << "Evaluation counts:\n";
	ss_ << "  Full: " << full_eval_;
	if( full_eval_ + endgame_eval_ > 0 ) {
		ss_ << " (" << 100 * static_cast<double>(full_eval_) / (full_eval_ + endgame_eval_) << "%)";
	}
	ss_ << "\n  Endgame: " << endgame_eval_;
	if( full_eval_ + endgame_eval_ > 0 ) {
		ss_ << " (" << 100 * static_cast<double>(endgame_eval_) / (full_eval_ + endgame_eval_) << "%)";
	}
	ss_ << "\n\n";
#else
	(void)ctx;
#endif

	dlog() << ss_.str();
}

void statistics::print_total()
{
	ss_.str( std::string() );

	ss_ << "\n";
	ss_ << "Node stats:\n";
	ss_ << "  Total:            " << std::setw(14) << total_full_width_nodes + total_quiescence_nodes << "\n";
	ss_ << "  Full-width:       " << std::setw(14) << total_full_width_nodes << "\n";
	ss_ << "  Quiescence:       " << std::setw(14) << total_quiescence_nodes << "\n";

	if( total_full_width_nodes || total_quiescence_nodes ) {
		if( !total_elapsed.empty() ) {
			ss_ << "  Nodes per second: " << std::setw(14) << std::setfill(' ') << total_elapsed.get_items_per_second(total_full_width_nodes + total_quiescence_nodes) << "\n";
		}
		ss_ << "  Time per node:    " << std::setw(11) << total_elapsed.nanoseconds() / (total_full_width_nodes + total_quiescence_nodes) << " ns\n";
	}

	ss_ << "\n";

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

#if USE_STATISTICS >= 2
	cutoffs_ += stats.cutoffs_;
	processed_ += stats.processed_;
#endif
}


void statistics::reset( bool total )
{
	for( int i = 0; i < MAX_DEPTH; ++i ) {
		full_width_nodes[i] = 0;
	}
	quiescence_nodes = 0;

#if USE_STATISTICS >= 2
	cutoffs_ = 0;
	processed_ = 0;

	full_eval_ = 0;
	endgame_eval_ = 0;
#endif

	if( total ) {
		total_full_width_nodes = 0;
		total_quiescence_nodes = 0;
		total_elapsed = duration();
	}
}

void statistics::node( int ply )
{
	add_relaxed( full_width_nodes[ply], 1 );
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
			ss_ << std::setw(11) << nodes << "\n";
		}
	}

	dlog() << ss_.str();
}


#if USE_STATISTICS >= 2
void statistics::add_cutoff( int processed )
{
	add_relaxed( processed_, processed );
	add_relaxed( cutoffs_, 1 );
}
#endif

#endif
