#include "pvlist.hpp"
#include "util.hpp"

#include <iostream>
#include <sstream>

pv_entry::pv_entry()
	: next_()
{
	best_move_.other = 0;
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
		ret->best_move_.other = 0;
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
	parent->best_move_.other = 1;
	parent->next_ = child;
}

void pv_entry_pool::set_pv_move( pv_entry* pv, move const& m )
{
	if( pv->next_ && pv->best_move_ != m ) {
		release( pv->next_ );
		pv->next_ = 0;
	}
	pv->best_move_ = m;
	pv->best_move_.other = 1;
}

void pv_entry_pool::clear_pv_move( pv_entry* pv )
{
	pv->best_move_.other = 0;
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
	while( pv && pv->get_best_move().other ) {
		ss << move_to_string( p, c, pv->get_best_move() ) << " ";
		bool captured;
		if( !apply_move( p, pv->get_best_move(), c, captured ) ) {
			ss << "FAIL! Invalid mode in pv: "
					  << static_cast<int>(pv->get_best_move().other) << " "
					  << static_cast<int>(pv->get_best_move().source_col) << " "
					  << static_cast<int>(pv->get_best_move().source_row) << " "
					  << static_cast<int>(pv->get_best_move().target_col) << " "
					  << static_cast<int>(pv->get_best_move().target_row) << " "
					  << std::endl;
			return ss.str();
		}

		c = static_cast<color::type>(1-c);

		pv = pv->next();
	}
	return ss.str();
}
