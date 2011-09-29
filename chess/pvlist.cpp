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
		ret->best_move_.flags = 0;
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
	pv->best_move_.flags = 0;
	if( pv->next() ) {
		release( pv->next_ );
		pv->next_ = 0;
	}
}

void print_pv( pv_entry const* pv, position p, color::type c )
{
	std::cerr << pv_to_string( pv, p, c ) << std::endl;
}

std::string pv_to_string( pv_entry const* pv, position p, color::type c )
{
	std::stringstream ss;
	while( pv && pv->get_best_move().flags & move_flags::valid ) {
		ss << move_to_string( p, c, pv->get_best_move() ) << " ";
		if( !apply_move( p, pv->get_best_move(), c ) ) {
			ss << "FAIL! Invalid mode in pv: "
					  << static_cast<int>(pv->get_best_move().flags) << " "
					  << static_cast<int>(pv->get_best_move().source % 8) << " "
					  << static_cast<int>(pv->get_best_move().source / 8) << " "
					  << static_cast<int>(pv->get_best_move().target % 8) << " "
					  << static_cast<int>(pv->get_best_move().target / 8) << " "
					  << std::endl;
			return ss.str();
		}

		c = static_cast<color::type>(1-c);

		pv = pv->next();
	}
	return ss.str();
}

void extend_pv_from_tt( pv_entry* pv, position p, color::type c, int max_depth, int max_qdepth )
{
	// There might be some exact nodes in the transposition table at the end
	// of the pv. Happens if search aborts early due to exact hit. Here we
	// thus recover a more complete pv.
	int depth = 0;
	pv_entry* prev = 0;
	while( pv && pv->get_best_move().flags & move_flags::valid ) {
		++depth;
		if( !apply_move( p, pv->get_best_move(), c ) ) {
			std::cerr << "FAIL! Invalid mode in pv: "
					  << static_cast<int>(pv->get_best_move().flags) << " "
					  << static_cast<int>(pv->get_best_move().source % 8) << " "
					  << static_cast<int>(pv->get_best_move().source / 8) << " "
					  << static_cast<int>(pv->get_best_move().target % 8) << " "
					  << static_cast<int>(pv->get_best_move().target / 8) << " "
					  << std::endl;
		}

		c = static_cast<color::type>(1-c);

		prev = pv;
		pv = pv->next();
	}

	if( !prev ) {
		return;
	}
	return; // FIXME

	while(true) {
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
		score_type::type s = transposition_table.lookup( hash, c, r, 0, result::loss, result::win, ev, best, 0 );
		if( s != score_type::exact || !(best.flags & move_flags::valid) ) {
			break;
		}

		if( !apply_move( p, best, c) ) {
			std::cerr << "FAIL" << std::endl;
		}
		c = static_cast<color::type>(1-c);

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
