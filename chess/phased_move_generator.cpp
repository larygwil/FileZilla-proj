#include "phased_move_generator.hpp"
#include "eval.hpp"
#include "fen.hpp"
#include "see.hpp"
#include "util.hpp"

#include <algorithm>
#include <iostream>
#include "random.hpp"

namespace {
void evaluate_noncaptures( context const& ctx, move_info* begin, move_info* end, position const& p )
{
	for( move_info* it = begin; it != end; ++it ) {
		it->sort = ctx.history_.get_value( p.get_piece( it->m ), it->m, p.self() );
	}
}
}


phased_move_generator_base::phased_move_generator_base( context& cntx, position const& p, check_map const& check )
	: ctx( cntx )
	, phase( phases::hash_move )
	, moves( ctx.move_ptr )
	, it( ctx.move_ptr )
	, p_(p)
	, check_(check)
	, bad_captures_end_(moves)
{
}


phased_move_generator_base::~phased_move_generator_base()
{
	ctx.move_ptr = moves;
}


qsearch_move_generator::qsearch_move_generator( context& cntx, position const& p, check_map const& check, bool pv_node, bool include_noncaptures )
	: phased_move_generator_base( cntx, p, check )
	, pv_node_( pv_node )
	, include_noncaptures_( include_noncaptures )
{
}

void get_best( move_info* begin, move_info* end ) {
	move_info* best = begin;
	for( move_info* it = begin + 1; it != end; ++it ) {
		if( it->sort > best->sort ) {
			best = it;
		}
	}

	// Bubble best to front
	for( ; best != begin; --best ) {
		std::swap( *(best-1), *best );
	}
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
#if CHECK_TYPE_1_COLLISION
			if( !is_valid_move( p_, hash_move, check_ ) ) {
				std::cerr << "Possible type-1 hash collision:" << std::endl;
				std::cerr << board_to_string( p_, color::white ) << std::endl;
				std::cerr << position_to_fen_noclock( p_ ) << std::endl;
				std::cerr << move_to_string( p_, hash_move ) << std::endl;
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
		calculate_moves_captures( p_, ctx.move_ptr, check_ );
		phase = phases::captures;
	case phases::captures:
		while( it != ctx.move_ptr ) {
			get_best( it, ctx.move_ptr );
			if( it->m == hash_move ) {
				++it;
				continue;
			}
#if DELAY_BAD_CAPTURES
			pieces::type piece = p_.get_piece( it->m );
			pieces::type captured_piece = p_.get_captured_piece( it->m );

			if( piece > captured_piece ) {
				int see_score = see( p_, it->m );
				if( see_score < 0 ) {
					if( check_.check || pv_node_ ) {
						*bad_captures_end_ = *it;
						(bad_captures_end_++)->sort = see_score;
					}
					++it;
					continue;
				}
			}
#else
			if( !check_.check && !pv_node_ && it->m.piece > it->m.captured_piece ) {
				int see_score = see( p_, it->m );
				if( see_score < 0 ) {
					++it;
					continue;
				}
			}
#endif

			return it++;
		}
		if( check_.check || include_noncaptures_ ) {
			phase = phases::noncaptures_gen;
		}
		else {
			phase = phases::done;
			return 0;
		}
	case phases::noncaptures_gen:
		ctx.move_ptr = bad_captures_end_;
		it = bad_captures_end_;
		if( check_.check ) {
			calculate_moves_noncaptures<false>( p_, ctx.move_ptr, check_ );
		}
		else {
			calculate_moves_noncaptures<true>( p_, ctx.move_ptr, check_ );
		}
		evaluate_noncaptures( ctx, bad_captures_end_, ctx.move_ptr, p_ );
		phase = phases::noncapture;
	case phases::noncapture:
		while( it != ctx.move_ptr ) {
			get_best( it, ctx.move_ptr );
			if( it->m == hash_move ) {
				++it;
				continue;
			}

			if( !check_.check && !pv_node_ ) {
				int see_score = see( p_, it->m );
				if( see_score < 0 ) {
					++it;
					continue;
				}
			}

			return it++;
		}
#if DELAY_BAD_CAPTURES
		phase = phases::bad_captures;
		it = moves;
		ctx.move_ptr = bad_captures_end_;
	case phases::bad_captures:
		while( it != bad_captures_end_ ) {
			get_best( it, bad_captures_end_ );
			return it++;
		}
#endif
		phase = phases::done;
	case phases::done:
	default:
		break;
	}

	return 0;
}


move_generator::move_generator( context& cntx, killer_moves const& killers, position const& p, check_map const& check )
	: phased_move_generator_base( cntx, p, check )
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
#if CHECK_TYPE_1_COLLISION
			if( !is_valid_move( p_, hash_move, check_ ) ) {
				std::cerr << "Possible type-1 hash collision:" << std::endl;
				std::cerr << board_to_string( p_, color::white ) << std::endl;
				std::cerr << position_to_fen_noclock( p_ ) << std::endl;
				std::cerr << move_to_string( p_, hash_move ) << std::endl;
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
		calculate_moves_captures( p_, ctx.move_ptr, check_ );
		phase = phases::captures;
	case phases::captures:
		while( it != ctx.move_ptr ) {
			get_best( it, ctx.move_ptr );
			if( it->m != hash_move ) {

#if DELAY_BAD_CAPTURES
				short s;
				pieces::type piece = p_.get_piece( it->m );
				pieces::type captured_piece = p_.get_captured_piece( it->m );
				if( piece > captured_piece && (s = see( p_, it->m )) < 0 ) {
					*bad_captures_end_ = *(it++);
					(bad_captures_end_++)->sort = s;
				}
				else
#endif
				{
					return it++;
				}
			}
			else {
				++it;
			}
		}
		phase = phases::killer1;
	case phases::killer1:
		phase = phases::killer2;
		ctx.move_ptr = bad_captures_end_ + 1;
		if( !killers_.m1.empty() && killers_.m1 != hash_move && !p_.get_captured_piece(killers_.m1) && is_valid_move( p_, killers_.m1, check_ ) ) {
			bad_captures_end_->m = killers_.m1;
			return bad_captures_end_;
		}
	case phases::killer2:
		phase = phases::noncaptures_gen;
		if( !killers_.m2.empty() && killers_.m2 != hash_move && killers_.m1 != killers_.m2 && !p_.get_captured_piece(killers_.m2) && is_valid_move( p_, killers_.m2, check_ ) ) {
			bad_captures_end_->m = killers_.m2;
			return bad_captures_end_;
		}
	case phases::noncaptures_gen:
		ctx.move_ptr = bad_captures_end_;
		it = bad_captures_end_;
		calculate_moves_noncaptures<false>( p_, ctx.move_ptr, check_ );
		evaluate_noncaptures( ctx, bad_captures_end_, ctx.move_ptr, p_ );
		phase = phases::noncapture;
	case phases::noncapture:
		while( it != ctx.move_ptr ) {
			get_best( it, ctx.move_ptr );
			if( it->m != hash_move && it->m != killers_.m1 && it->m != killers_.m2 ) {
				return it++;
			}
			else {
				++it;
			}
		}
#if DELAY_BAD_CAPTURES
		phase = phases::bad_captures;
		it = moves;
		ctx.move_ptr = bad_captures_end_;
	case phases::bad_captures:
		while( it != bad_captures_end_ ) {
			get_best( it, bad_captures_end_ );
			return it++;
		}
#endif
		phase = phases::done;
	case phases::done:
		break;
	}

	return 0;
}

void move_generator::update_history()
{
	if( phase == phases::noncapture ) {
		ctx.history_.record_cut( p_, bad_captures_end_, it, p_.self() );
	}
}
