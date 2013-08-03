#include "phased_move_generator.hpp"
#include "calc.hpp"
#include "eval.hpp"
#include "fen.hpp"
#include "see.hpp"
#include "util.hpp"

#include <algorithm>
#include <iostream>

namespace {
void evaluate_noncaptures( calc_state const& state, move_info* begin, move_info* end, position const& p )
{
	for( move_info* it = begin; it != end; ++it ) {
		it->sort = state.history_.get_value( p.get_piece( it->m ), it->m, p.self() );
	}
}
}


phased_move_generator_base::phased_move_generator_base( calc_state& state, position const& p, check_map const& check )
	: state_( state )
	, phase( phases::hash_move )
	, moves( state_.move_ptr )
	, it( state_.move_ptr )
	, p_(p)
	, check_(check)
	, bad_captures_end_(moves)
{
}


phased_move_generator_base::~phased_move_generator_base()
{
	state_.move_ptr = moves;
}


qsearch_move_generator::qsearch_move_generator( calc_state& cntx, position const& p, check_map const& check, bool pv_node, bool include_noncaptures )
	: phased_move_generator_base( cntx, p, check )
	, pv_node_( pv_node )
	, include_noncaptures_( include_noncaptures )
{
}

void sort( move_info* begin, move_info* end ) {
	// Insertion sort
	for( move_info* it = begin + 1; it < end; ++it ) {
		move_info tmp = *it;
		move_info* it2 = it;
		while( it2 != begin && (it2-1)->sort < tmp.sort ) {
			*it2 = *(it2-1);
			--it2;
		}
		*it2 = tmp;
	}
}

// Returns the next legal move.
// move_info's m, evaluation and pawns are filled out, sort is undefined.
move qsearch_move_generator::next()
{
	// The fall-throughs are on purpose
	switch( phase ) {
	case phases::hash_move:
		phase = phases::captures_gen;
		if( !hash_move.empty() ) {
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
				return hash_move;
			}
		}
	case phases::captures_gen:
		calculate_moves<movegen_type::capture>( p_, state_.move_ptr, check_ );
		phase = phases::captures;
		sort( it, state_.move_ptr );
	case phases::captures:
		while( it != state_.move_ptr ) {
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
			return (it++)->m;
		}

		if( check_.check || include_noncaptures_ ) {
			state_.move_ptr = bad_captures_end_;
			it = bad_captures_end_;
			if( check_.check ) {
				calculate_moves<movegen_type::noncapture>( p_, state_.move_ptr, check_ );
			}
			else {
				calculate_moves<movegen_type::pseudocheck>( p_, state_.move_ptr, check_ );
			}
			evaluate_noncaptures( state_, bad_captures_end_, state_.move_ptr, p_ );
			sort( it, state_.move_ptr );
			phase = phases::noncapture;
		}
		//Fall-through
	case phases::noncapture:
		while( it != state_.move_ptr ) {
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

			return (it++)->m;
		}
#if DELAY_BAD_CAPTURES
		phase = phases::bad_captures;
		it = moves;
		state_.move_ptr = bad_captures_end_;
		sort( it, bad_captures_end_ );
	case phases::bad_captures:
		while( it != bad_captures_end_ ) {
			return (it++)->m;
		}
#endif
		phase = phases::done;
	case phases::done:
	default:
		break;
	}

	return move();
}


move_generator::move_generator( calc_state& cntx, killer_moves const& killers, position const& p, check_map const& check )
	: phased_move_generator_base( cntx, p, check )
	, killers_(killers)
{
}


// Returns the next legal move.
// move_info's m, evaluation and pawns are filled out, sort is undefined.
move move_generator::next() {
	// The fall-throughs are on purpose
	switch( phase ) {
	case phases::hash_move:
		phase = phases::captures_gen;
		if( !hash_move.empty() ) {
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
				return hash_move;
			}
		}
	case phases::captures_gen:
		calculate_moves<movegen_type::capture>( p_, state_.move_ptr, check_ );
		phase = phases::captures;
		sort( it, state_.move_ptr );
	case phases::captures:
		while( it != state_.move_ptr ) {
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
					return (it++)->m;
				}
			}
			else {
				++it;
			}
		}
		state_.move_ptr = bad_captures_end_;
		phase = phases::killer1;
	case phases::killer1:
		phase = phases::killer2;
		if( !killers_.m1.empty() && killers_.m1 != hash_move && !p_.get_captured_piece(killers_.m1) && is_valid_move( p_, killers_.m1, check_ ) ) {
			return killers_.m1;
		}
	case phases::killer2:
		phase = phases::noncaptures_gen;
		if( !killers_.m2.empty() && killers_.m2 != hash_move && killers_.m1 != killers_.m2 && !p_.get_captured_piece(killers_.m2) && is_valid_move( p_, killers_.m2, check_ ) ) {
			return killers_.m2;
		}
	case phases::noncaptures_gen:
		it = bad_captures_end_;
		calculate_moves<movegen_type::noncapture>( p_, state_.move_ptr, check_ );
		evaluate_noncaptures( state_, bad_captures_end_, state_.move_ptr, p_ );
		phase = phases::noncapture;
		sort( it, state_.move_ptr );
	case phases::noncapture:
		while( it != state_.move_ptr ) {
			if( it->m != hash_move && it->m != killers_.m1 && it->m != killers_.m2 ) {
				return (it++)->m;
			}
			else {
				++it;
			}
		}
#if DELAY_BAD_CAPTURES
		phase = phases::bad_captures;
		it = moves;
		state_.move_ptr = bad_captures_end_;
		sort( it, bad_captures_end_ );
	case phases::bad_captures:
		while( it != bad_captures_end_ ) {
			return (it++)->m;
		}
#endif
		phase = phases::done;
	case phases::done:
		break;
	}

	return move();
}

void move_generator::update_history()
{
	if( phase == phases::noncapture ) {
		state_.history_.record_cut( p_, bad_captures_end_, it, p_.self() );
	}
}
