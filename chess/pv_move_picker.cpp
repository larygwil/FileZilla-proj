#include "assert.hpp"
#include "pv_move_picker.hpp"
#include "util.hpp"
#include "zobrist.hpp"

pv_move_picker::pv_move_picker()
	: hash_()
{
}


move pv_move_picker::can_use_move_from_pv( position const& p, color::type c )
{
	move ret;

	if( get_zobrist_hash( p, c ) == hash_ ) {

		// Recapture
		if( previous_.captured_piece != pieces::none && next_.captured_piece != pieces::none && previous_.target == next_.target ) {
			ret = next_;
		}
	}

	return ret;
}


void pv_move_picker::update_pv( position p, color::type c, pv_entry const* pv )
{
	if( pv && pv->next() && pv->next()->next() ) {
		ASSERT( !pv->get_best_move().empty() );
		ASSERT( !pv->next()->get_best_move().empty() );
		ASSERT( !pv->next()->next()->get_best_move().empty() );

		apply_move( p, pv->get_best_move(), c );
		apply_move( p, pv->next()->get_best_move(), static_cast<color::type>(1-c) );

		hash_ = get_zobrist_hash( p, c );
		previous_ = pv->next()->get_best_move();
		next_ = pv->next()->next()->get_best_move();
	}
}
