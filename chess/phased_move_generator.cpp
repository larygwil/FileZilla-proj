#include "phased_move_generator.hpp"
#include "eval.hpp"
#include "see.hpp"
#include "util.hpp"

#include <algorithm>

namespace {
void sort_moves_noncaptures( move_info* begin, move_info* end, position const& p, color::type c )
{
	for( move_info* it = begin; it != end; ++it ) {
		it->sort = evaluate_move( p, c, it->m );
	}
	std::sort( begin, end, moveSort );
}
}


phased_move_generator_base::phased_move_generator_base( context& cntx, position const& p, color::type const& c, check_map const& check )
	: ctx( cntx )
	, phase( phases::hash_move )
	, moves( ctx.move_ptr )
	, it( ctx.move_ptr )
	, p_(p)
	, c_(c)
	, check_(check)
{
}

phased_move_generator_base::~phased_move_generator_base()
{
	ctx.move_ptr = moves;
}


qsearch_move_generator::qsearch_move_generator( context& cntx, position const& p, color::type const& c, check_map const& check, bool pv_node )
	: phased_move_generator_base( cntx, p, c, check )
	, pv_node_( pv_node )
{
}


// Returns the next legal move.
// move_info's m, evaluation and pawns are filled out, sort is undefined.
move_info const* qsearch_move_generator::next()
{
	// The fall-throughs are on purpose
	switch( phase ) {
	case phases::hash_move:
		phase = phases::captures_gen;
		if( !hash_move.empty() ) {
			ctx.move_ptr = moves + 1;
#if 0
			if( !is_valid_move( p_, c_, hash_move, check_ ) ) {
				std::cerr << "Possible type-1 hash collision:" << std::endl;
				std::cerr << board_to_string( p_ ) << std::endl;
				std::cerr << position_to_fen_noclock( p_, c_ ) << std::endl;
				std::cerr << move_to_string( hash_move ) << std::endl;
			}
			else
#endif
			{
				moves->m = hash_move;
				return moves;
			}
		}
	case phases::captures_gen:
		ctx.move_ptr = moves;
		calculate_moves_captures( p_, c_, ctx.move_ptr, check_ );
		std::sort( moves, ctx.move_ptr, moveSort );
		phase = phases::captures;
	case phases::captures:
		while( it != ctx.move_ptr ) {

			if( !check_.check ) {
				if( !pv_node_ && it->m.piece > it->m.captured_piece ) {
					int see_score = see( p_, c_, it->m );
					if( see_score < 0 ) {
						++it;
						continue;
					}
				}
			}

			return it++;
		}
		if( check_.check ) {
			phase = phases::noncaptures_gen;
		}
		else {
			phase = phases::done;
			return 0;
		}
	case phases::noncaptures_gen:
		ctx.move_ptr = moves;
		it = moves;
		calculate_moves_noncaptures( p_, c_, ctx.move_ptr, check_ );
		sort_moves_noncaptures( moves, ctx.move_ptr, p_, c_ );
		phase = phases::noncapture;
	case phases::noncapture:
		while( it != ctx.move_ptr ) {
			if( it->m != hash_move ) {
				return it++;
			}
			else {
				++it;
			}
		}
		phase = phases::done;
	case phases::done:
	default:
		break;
	}

	return 0;
}


move_generator::move_generator( context& cntx, killer_moves const& killers, position const& p, color::type const& c, check_map const& check )
	: phased_move_generator_base( cntx, p, c, check )
	, killers_(killers)
{
}


// Returns the next legal move.
// move_info's m, evaluation and pawns are filled out, sort is undefined.
move_info const* move_generator::next() {
	// The fall-throughs are on purpose
	switch( phase ) {
	case phases::hash_move:
		phase = phases::captures_gen;
		if( !hash_move.empty() ) {
			ctx.move_ptr = moves + 1;
#if 0
			if( !is_valid_move( p_, c_, hash_move, check_ ) ) {
				std::cerr << "Possible type-1 hash collision:" << std::endl;
				std::cerr << board_to_string( p_ ) << std::endl;
				std::cerr << position_to_fen_noclock( p_, c_ ) << std::endl;
				std::cerr << move_to_string( hash_move ) << std::endl;
			}
			else
#endif
			{
				moves->m = hash_move;
				return moves;
			}
		}
	case phases::captures_gen:
		ctx.move_ptr = moves;
		calculate_moves_captures( p_, c_, ctx.move_ptr, check_ );
		std::sort( moves, ctx.move_ptr, moveSort );
		phase = phases::captures;
	case phases::captures:
		if( it != ctx.move_ptr ) {
			return it++;
		}
		phase = phases::killer1;
	case phases::killer1:
		phase = phases::killer2;
		ctx.move_ptr = moves + 1;
		if( !killers_.m1.empty() && killers_.m1 != hash_move && is_valid_move( p_, c_, killers_.m1, check_ ) ) {
			moves->m = killers_.m1;
			moves->sort = evaluate_move( p_, c_, moves->m );
			return moves;
		}
	case phases::killer2:
		phase = phases::noncaptures_gen;
		if( !killers_.m2.empty() && killers_.m2 != hash_move && killers_.m1 != killers_.m2 && is_valid_move( p_, c_, killers_.m2, check_ ) ) {
			moves->m = killers_.m2;
			moves->sort = evaluate_move( p_, c_, moves->m );
			return moves;
		}
	case phases::noncaptures_gen:
		ctx.move_ptr = moves;
		it = moves;
		calculate_moves_noncaptures( p_, c_, ctx.move_ptr, check_ );
		sort_moves_noncaptures( moves, ctx.move_ptr, p_, c_ );
		phase = phases::noncapture;
	case phases::noncapture:
		while( it != ctx.move_ptr ) {
			if( it->m != hash_move && it->m != killers_.m1 && it->m != killers_.m2 ) {
				return it++;
			}
			else {
				++it;
			}
		}
		phase = phases::done;
	case phases::done:
		break;
	}

	return 0;
}
