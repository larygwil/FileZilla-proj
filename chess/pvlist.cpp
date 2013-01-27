#include "assert.hpp"
#include "config.hpp"
#include "hash.hpp"
#include "pvlist.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <iostream>
#include <sstream>

pv_entry::pv_entry()
	: next_()
{
	memset(&best_move_, 0, sizeof(move));
}

pv_entry_pool::pv_entry_pool()
	: first_free_()
	, last_free_()
{
}

pv_entry_pool::~pv_entry_pool()
{
	while( first_free_ ) {
		pv_entry* cur = first_free_;
		first_free_ = first_free_->next_;
		delete cur;
	}
	last_free_ = 0;
}

pv_entry* pv_entry_pool::get()
{
	pv_entry* ret;
	if( first_free_ ) {
		ret = first_free_;
		first_free_ = first_free_->next_;
		if( !first_free_ ) {
			last_free_ = 0;
		}
		ret->next_ = 0;
		ret->best_move_.piece = pieces::none;
	}
	else {
		ret = new pv_entry();
	}

	return ret;
}


void pv_entry_pool::release( pv_entry* pv )
{
	if( last_free_ ) {
		last_free_->next_ = pv;
	}
	else {
		first_free_ = pv;
	}
	last_free_ = pv;
	while( last_free_->next_ ) {
		last_free_ = last_free_->next_;
	}
}

void pv_entry_pool::append( pv_entry* parent, move const& best_move, pv_entry* child )
{
	ASSERT( best_move.empty() == (child == 0));
	if( parent->next_ ) {
		release( parent->next_ );
	}
	parent->best_move_ = best_move;
	parent->next_ = child;
}

void pv_entry_pool::set_pv_move( pv_entry* pv, move const& m )
{
	if( pv->next_ && pv->best_move_ != m ) {
		release( pv->next_ );
		pv->next_ = 0;
	}
	pv->best_move_ = m;
}

void pv_entry_pool::clear_pv_move( pv_entry* pv )
{
	pv->best_move_.piece = pieces::none;
	if( pv->next() ) {
		release( pv->next_ );
		pv->next_ = 0;
	}
}


std::string pv_to_string( pv_entry const* pv, position p, bool use_long_algebraic_notation )
{
	std::stringstream ss;
	while( pv && !pv->get_best_move().empty() ) {
		ss << 
			( use_long_algebraic_notation ?
				move_to_long_algebraic( pv->get_best_move() )
				: move_to_string( pv->get_best_move() ) ) 
		   << " ";

		apply_move( p, pv->get_best_move() );

		pv = pv->next();
	}
	return ss.str();
}

void extend_pv_from_tt( pv_entry* pv, position p, int max_depth, int max_qdepth )
{
	// There might be some exact nodes in the transposition table at the end
	// of the pv. Happens if search aborts early due to exact hit. Here we
	// thus recover a more complete pv.
	int depth = 0;
	pv_entry* prev = 0;
	while( pv && !pv->get_best_move().empty() ) {
		ASSERT( is_valid_move( p, pv->get_best_move(), check_map(p) ) );
		++depth;
		apply_move( p, pv->get_best_move() );

		prev = pv;
		pv = pv->next();
	}

	if( !prev ) {
		return;
	}

	while( depth < max_depth ) {
		uint64_t hash = get_zobrist_hash( p );

		int r;
		if( depth >= max_depth ) {
			r = max_depth + max_qdepth - depth;
		}
		else {
			r = max_depth - depth + 128;
		}
		move best;
		short ev;
		short full_eval;
		transposition_table.lookup( hash, r, 0, result::loss, result::win, ev, best, full_eval );
		if( best.empty() ) {
			break;
		}

		if( !is_valid_move( p, best, check_map(p) ) ) {
			break;
		}

		apply_move( p, best );

		pv_entry_pool pool;
		if( pv ) {
			pool.set_pv_move( pv, best );
		}
		else {
			pv = pool.get();
			pool.set_pv_move( pv, best );
			pool.append( prev, prev->get_best_move(), pv );
		}

		++depth;

		prev = pv;
		pv = pv->next();
	}
}


pv_entry* pv_entry_pool::clone( pv_entry const* pv )
{
	pv_entry* ret = 0;

	if( pv ) {
		ret = get();
		pv_entry* cur = ret;
		cur->best_move_ = pv->get_best_move();
		while( pv->next() ) {
			cur->next_ = get();
			cur = cur->next_;
			pv = pv->next_;
			cur->best_move_ = pv->get_best_move();
		}
	}

	return ret;
}
