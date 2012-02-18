#include "assert.hpp"
#include "moves.hpp"
#include "eval.hpp"
#include "magic.hpp"
#include "util.hpp"
#include "calc.hpp"
#include "sliding_piece_attacks.hpp"
#include "tables.hpp"

#include <algorithm>
#include <iostream>
#include <string>

extern unsigned long long const pawn_enpassant[2] = {
	0x0000ff0000000000ull,
	0x0000000000ff0000ull
};

namespace {

void do_add_move( move_info*& moves, pieces::type const& pi,
				  unsigned char const& source, unsigned char const& target,
				  int flags, pieces::type captured, promotions::type promotion = promotions::queen )
{
	move_info& mi= *(moves++);

	mi.m.flags = flags;
	mi.m.piece = pi;
	mi.m.source = source;
	mi.m.target = target;
	mi.m.captured_piece = captured;
	mi.m.promotion = promotion;
}

// Adds the move if it does not result in self getting into check
void add_if_legal( move_info*& moves, check_map const& check,
				  pieces::type const& pi,
				  unsigned char const& source, unsigned char const& target,
				  int flags, pieces::type captured, promotions::type promotion = promotions::queen )
{
	unsigned char const& cv_old = check.board[source];
	unsigned char const& cv_new = check.board[target];
	if( check.check ) {
		if( cv_old ) {
			// Can't come to rescue, this piece is already blocking yet another check.
			return;
		}
		if( cv_new != check.check ) {
			// Target position does capture checking piece nor blocks check
			return;
		}
	}
	else {
		if( cv_old && cv_old != cv_new ) {
			return;
		}
	}

	do_add_move( moves, pi, source, target, flags, captured, promotion );
}

void add_if_legal_king( position const& p, color::type c,
						move_info*& moves, unsigned char const& source, unsigned char const& target,
						int flags, pieces::type captured )
{
	if( detect_check( p, c, target, source ) ) {
		return;
	}

	do_add_move( moves, pieces::king, source, target, flags, captured );
}

void calc_moves_king( position const& p, color::type c, move_info*& moves,
					  unsigned char const& source, unsigned char const& target )
{
	pieces::type captured = get_piece_on_square( p, static_cast<color::type>(1-c), target );
	add_if_legal_king( p, c, moves, source, target, move_flags::valid, captured );
}


void calc_moves_king( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long kings = p.bitboards[c].b[bb_type::king];
	unsigned long long king;
	bitscan(kings, king);

	unsigned long long other_kings = p.bitboards[1-c].b[bb_type::king];
	unsigned long long other_king;
	bitscan(other_kings, other_king);

	unsigned long long king_moves = possible_king_moves[king] & ~(p.bitboards[c].b[bb_type::all_pieces] | possible_king_moves[other_king]);
	while( king_moves ) {
		unsigned long long king_move;
		bitscan( king_moves, king_move );
		king_moves &= king_moves - 1;
		calc_moves_king( p, c, moves,
						 king, king_move );
	}

	if( check.check ) {
		return;
	}

	unsigned char row = c ? 56 : 0;
	// Queenside castling
	if( p.castle[c] & 0x2 ) {
		if( p.get_occupancy( 0xeull << row ) == 0 && !(possible_king_moves[2 + row] & other_kings ) ) {
			if( !detect_check( p, c, 3 + row, 3 + row ) ) {
				add_if_legal_king( p, c, moves, 4 + row, 2 + row, move_flags::valid | move_flags::castle, pieces::none );
			}
		}
	}
	// Kingside castling
	if( p.castle[c] & 0x1 ) {
		if( p.get_occupancy( 0x60ull << row ) == 0 && !(possible_king_moves[6 + row] & other_kings ) ) {
			if( !detect_check( p, c, 5 + row, 5 + row ) ) {
				add_if_legal_king( p, c, moves, 4 + row, 6 + row, move_flags::valid | move_flags::castle, pieces::none );
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, move_info*& moves, check_map const& check, unsigned long long queen )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = rook_magic( queen, all_blockers ) | bishop_magic( queen, all_blockers );
	possible_moves &= ~p.bitboards[c].b[bb_type::all_pieces];

	while( possible_moves ) {
		unsigned long long queen_move;
		bitscan( possible_moves, queen_move );
		possible_moves &= possible_moves - 1;

		pieces::type captured = get_piece_on_square( p, static_cast<color::type>(1-c), queen_move );
		add_if_legal( moves, check, pieces::queen, queen, queen_move, move_flags::valid, captured );
	}
}


void calc_moves_queens( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long queens = p.bitboards[c].b[bb_type::queens];
	while( queens ) {
		unsigned long long queen;
		bitscan( queens, queen );	
		queens &= queens - 1;
		calc_moves_queen( p, c, moves, check, queen );
	}
}


void calc_moves_bishop( position const& p, color::type c, move_info*& moves, check_map const& check,
					    unsigned long long bishop )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = bishop_magic( bishop, all_blockers );
	possible_moves &= ~p.bitboards[c].b[bb_type::all_pieces];

	while( possible_moves ) {
		unsigned long long bishop_move;
		bitscan( possible_moves, bishop_move );
		possible_moves &= possible_moves - 1;

		pieces::type captured = get_piece_on_square( p, static_cast<color::type>(1-c), bishop_move );
		add_if_legal( moves, check, pieces::bishop, bishop, bishop_move, move_flags::valid, captured );
	}
}


void calc_moves_bishops( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long bishops = p.bitboards[c].b[bb_type::bishops];
	while( bishops ) {
		unsigned long long bishop;
		bitscan( bishops, bishop );	
		bishops &= bishops - 1;
		calc_moves_bishop( p, c, moves, check, bishop );
	}
}


void calc_moves_rook( position const& p, color::type c, move_info*& moves, check_map const& check,
					  unsigned long long rook )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = rook_magic( rook, all_blockers );
	possible_moves &= ~p.bitboards[c].b[bb_type::all_pieces];

	while( possible_moves ) {
		unsigned long long rook_move;
		bitscan( possible_moves, rook_move );
		possible_moves &= possible_moves - 1;

		pieces::type captured = get_piece_on_square( p, static_cast<color::type>(1-c), rook_move );
		add_if_legal( moves, check, pieces::rook, rook, rook_move, move_flags::valid, captured );
	}
}


void calc_moves_rooks( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long rooks = p.bitboards[c].b[bb_type::rooks];
	while( rooks ) {
		unsigned long long rook;
		bitscan( rooks, rook );
		rooks &= rooks - 1;
		calc_moves_rook( p, c, moves, check, rook );
	}
}


void calc_moves_knight( position const& p, color::type c, move_info*& moves, check_map const& check,
						unsigned char source, unsigned char target )
{
	pieces::type captured = get_piece_on_square( p, static_cast<color::type>(1-c), target );
	add_if_legal( moves, check, pieces::knight, source, target, move_flags::valid, captured );
}

void calc_moves_knight( position const& p, color::type c, move_info*& moves, check_map const& check,
						unsigned long long old_knight )
{
	unsigned long long new_knights = possible_knight_moves[old_knight] & ~(p.bitboards[c].b[bb_type::all_pieces]);
	while( new_knights ) {
		unsigned long long new_knight;
		bitscan( new_knights, new_knight );
		new_knights &= new_knights - 1;
		calc_moves_knight( p, c, moves, check,
						   old_knight, new_knight );
	}
}


void calc_moves_knights( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long knights = p.bitboards[c].b[bb_type::knights];
	while( knights ) {
		unsigned long long knight;
		bitscan( knights, knight );
		knights &= knights - 1;
		calc_moves_knight( p, c, moves, check, knight );
	}
}


void calc_moves_pawn_en_passant( position const& p, color::type c, move_info*& moves, check_map const& check,
								 unsigned long long pawn )
{
	unsigned long long enpassantable = 1ull << p.can_en_passant;
	unsigned long long enpassants = pawn_control[c][pawn] & enpassantable & pawn_enpassant[c];
	if( enpassants ) {
		unsigned long long enpassant;
		bitscan( enpassants, enpassant );

		unsigned char new_col = enpassant % 8;

		unsigned char old_col = static_cast<unsigned char>(pawn % 8);
		unsigned char old_row = static_cast<unsigned char>(pawn / 8);

		// Special case: Cannot use normal check from add_if_legal as target square is not piece square and if captured pawn gives check, bad things happen.
		unsigned char const& cv_old = check.board[pawn];
		unsigned char const& cv_new = check.board[enpassant];
		if( check.check ) {
			if( cv_old ) {
				// Can't come to rescue, this piece is already blocking yet another check.
				return;
			}
			if( cv_new != check.check && check.check != (0x80 + new_col + old_row * 8) ) {
				// Target position does capture checking piece nor blocks check
				return;
			}
		}
		else {
			if( cv_old && cv_old != cv_new ) {
				return;
			}
		}

		// Special case: black queen, black pawn, white pawn, white king from left to right on rank 5. Capturing opens up check!
		unsigned long long kings = p.bitboards[c].b[bb_type::king];
		unsigned long long king;
		bitscan(kings, king);
		unsigned char king_col = static_cast<unsigned char>(king % 8);
		unsigned char king_row = static_cast<unsigned char>(king / 8);

		if( king_row == old_row ) {
			signed char cx = static_cast<signed char>(old_col) - king_col;
			if( cx > 0 ) {
				cx = 1;
			}
			else {
				cx = -1;
			}
			for( signed char col = old_col + cx; col < 8 && col >= 0; col += cx ) {
				if( col == new_col ) {
					continue;
				}

				if( p.bitboards[c].b[bb_type::all_pieces] & (1ull << (col + old_row * 8 ) ) ) {
					// Own piece
					continue;
				}
				
				pieces::type t = get_piece_on_square( p, static_cast<color::type>(1-c), col + old_row * 8 );

				if( t == pieces::none ) {
					continue;
				}

				if( t == pieces::queen || t == pieces::rook ) {
					// Not a legal move unfortunately
					return;
				}

				// Harmless piece
				break;
			}
		}

		do_add_move( moves, pieces::pawn, pawn, enpassant, move_flags::valid | move_flags::enpassant, pieces::pawn );
	}
}


void calc_moves_pawn( position const& p, color::type c, move_info*& moves, check_map const& check,
					  unsigned long long pawn )
{
	unsigned long long pawn_captures = pawn_control[c][pawn] & p.bitboards[1-c].b[bb_type::all_pieces];
	while( pawn_captures ) {
		unsigned long long pawn_move;
		bitscan( pawn_captures, pawn_move );
		pawn_captures &= pawn_captures - 1;

		pieces::type captured = get_piece_on_square( p, static_cast<color::type>(1-c), pawn_move );

		if( pawn_move / 8 == ( c ? 0 : 7 ) )  {
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::valid|move_flags::promotion, captured, promotions::queen );
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::valid|move_flags::promotion, captured, promotions::rook );
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::valid|move_flags::promotion, captured, promotions::bishop );
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::valid|move_flags::promotion, captured, promotions::knight );
		}
		else {
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::valid, captured );
		}
	}

	calc_moves_pawn_en_passant( p, c, moves, check, pawn );

	unsigned char old_col = static_cast<unsigned char>(pawn % 8);
	unsigned char old_row = static_cast<unsigned char>(pawn / 8);

	unsigned char new_row = (c == color::white) ? (old_row + 1) : (old_row - 1);
	if( !p.is_occupied_square( old_col + new_row * 8 ) ) {
		unsigned char pawn_move = old_col + new_row * 8;
		if( new_row == 0 || new_row == 7 ) {
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::valid|move_flags::promotion, pieces::none, promotions::queen );
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::valid|move_flags::promotion, pieces::none, promotions::rook );
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::valid|move_flags::promotion, pieces::none, promotions::bishop );
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::valid|move_flags::promotion, pieces::none, promotions::knight );
		}
		else {
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::valid, pieces::none );
		}

		if( old_row == ( (c == color::white) ? 1 : 6) ) {
			// Moving two rows from starting row
			new_row = (c == color::white) ? (old_row + 2) : (old_row - 2);

			if( !p.is_occupied_square( old_col + new_row * 8 ) ) {
				add_if_legal( moves, check, pieces::pawn, pawn, old_col + new_row * 8, move_flags::valid | move_flags::pawn_double_move, pieces::none );
			}
		}
	}
}

void calc_moves_pawns( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long pawns = p.bitboards[c].b[bb_type::pawns];
	while( pawns ) {
		unsigned long long pawn;
		bitscan( pawns, pawn );
		pawns &= pawns - 1;

		calc_moves_pawn( p, c, moves, check, pawn );
	}
}
}

void calculate_moves( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	calc_moves_king( p, c, moves, check );

	if( !check.check || !check.multiple() )
	{
		calc_moves_pawns( p, c, moves, check );
		calc_moves_queens( p, c, moves, check );
		calc_moves_rooks( p, c, moves, check );
		calc_moves_bishops( p, c, moves, check );
		calc_moves_knights( p, c, moves, check );
	}
}
