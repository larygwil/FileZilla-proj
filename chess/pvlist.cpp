#include "assert.hpp"
#include "config.hpp"
#include "hash.hpp"
#include "pvlist.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <iostream>
#include <sstream>

std::string pv_to_string( move const* pv, position p, bool use_long_algebraic_notation )
{
	std::stringstream ss;
	while( pv && !pv->empty() ) {
		ss << 
			( use_long_algebraic_notation ?
				move_to_long_algebraic( *pv )
				: move_to_string( p, *pv ) ) 
		   << " ";

		apply_move( p, *pv );

		++pv;
	}
	return ss.str();
}

void get_pv_from_tt( move* pv, position p, int max_depth )
{
	ASSERT( pv && !pv->empty() );
	apply_move( p, *pv );

	++pv;

	uint64_t hash = get_zobrist_hash( p );

	for( int depth = 1; depth < max_depth; ++depth ) {

		move best;
		short ev;
		short full_eval;
		transposition_table.lookup( hash, 0, 0, result::loss, result::win, ev, best, full_eval );
		if( best.empty() ) {
			break;
		}

		if( !is_valid_move( p, best, check_map(p) ) ) {
			break;
		}

		hash = update_zobrist_hash( p, hash, best );
		apply_move( p, best );

		*pv = best;
		++pv;
	}

	pv->clear();
}

void push_pv_to_tt( move const* pv, position p, int clock )
{
	ASSERT( pv && !pv->empty() );

	apply_move( p, *pv );
	++pv;
	int ply = 1;

	uint64_t hash = get_zobrist_hash( p );

	while( !pv->empty() ) {

		move best;
		short ev;
		short full_eval;
		transposition_table.lookup( hash, 0, 0, result::loss, result::win, ev, best, full_eval );
		if( best != *pv ) {
			std::cerr << "Push " << move_to_string(p, *pv) << " " << move_to_string(p, best) << " " << ply << std::endl;
			transposition_table.store( hash, 0, ply, 0, 0, 0, *pv, clock, result::win );
		}

		hash = update_zobrist_hash( p, hash, *pv );
		apply_move( p, *pv );
		++pv;
		++ply;
	}
}