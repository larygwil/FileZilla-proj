#include "assert.hpp"
#include "pv_move_picker.hpp"
#include "util.hpp"
#include "zobrist.hpp"

pv_move_picker::pv_move_picker()
	: hash_()
{
}


std::pair<move,move> pv_move_picker::can_use_move_from_pv( position const& p )
{
	std::pair<move,move> ret;

	if( get_zobrist_hash( p ) == hash_ ) {

		// Recapture
		if( previous_pos_.get_captured_piece(previous_) != pieces::none && next_pos_.get_captured_piece(next_) != pieces::none && previous_.target() == next_.target() ) {
			ret.first = next_;
			ret.second = ponder_;
		}
	}

	return ret;
}


void pv_move_picker::update_pv( position p, pv_entry const* pv )
{
	if( pv && pv->next() && pv->next()->next() ) {
		ASSERT( !pv->get_best_move().empty() );
		ASSERT( !pv->next()->get_best_move().empty() );
		ASSERT( !pv->next()->next()->get_best_move().empty() );

		apply_move( p, pv->get_best_move() );
		previous_pos_ = p;
		apply_move( p, pv->next()->get_best_move() );
		next_pos_ = p;

		hash_ = get_zobrist_hash( p );
		previous_ = pv->next()->get_best_move();
		next_ = pv->next()->next()->get_best_move();

		if( pv->next()->next()->next() ) {
			ponder_ = pv->next()->next()->next()->get_best_move();
		}
		else {
			ponder_.clear();
		}
	}
	else {
		ponder_.clear();
	}
}
