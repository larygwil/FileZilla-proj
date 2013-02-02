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


void pv_move_picker::update_pv( position p, move const* pv )
{
	if( pv && !pv->empty() && !pv[1].empty() && !pv[2].empty() ) {
		apply_move( p, *pv );
		previous_pos_ = p;
		apply_move( p, pv[1] );
		next_pos_ = p;

		hash_ = get_zobrist_hash( p );
		previous_ = pv[1];
		next_ = pv[2];

		ponder_ = pv[3];
	}
	else {
		ponder_.clear();
	}
}
