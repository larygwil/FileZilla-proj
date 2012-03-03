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

namespace {

void do_add_move( move_info*& moves, pieces::type const& pi,
				  unsigned char const& source, unsigned char const& target,
				  int flags )
{
	move_info& mi= *(moves++);

	mi.m.flags = flags;
	mi.m.piece = pi;
	mi.m.source = source;
	mi.m.target = target;
	mi.m.captured_piece = pieces::none;
}

// Adds the move if it does not result in self getting into check
void add_if_legal( move_info*& moves, check_map const& check,
				  pieces::type const& pi,
				  unsigned char const& source, unsigned char const& target,
				  int flags )
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

	do_add_move( moves, pi, source, target, flags );
}

void add_if_legal_king( position const& p, color::type c,
						move_info*& moves, unsigned char const& source, unsigned char const& target,
						int flags )
{
	if( detect_check( p, c, target, source ) ) {
		return;
	}

	do_add_move( moves, pieces::king, source, target, flags );
}

void calc_moves_king( position const& p, color::type c, move_info*& moves,
					  unsigned char const& source, unsigned char const& target )
{
	add_if_legal_king( p, c, moves, source, target, move_flags::none );
}


void calc_moves_king( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long kings = p.bitboards[c].b[bb_type::king];
	unsigned long long king = bitscan( kings );

	unsigned long long other_kings = p.bitboards[1-c].b[bb_type::king];
	unsigned long long other_king = bitscan( other_kings );

	unsigned long long king_moves = possible_king_moves[king] & ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces] | possible_king_moves[other_king]);
	while( king_moves ) {
		unsigned long long king_move = bitscan_unset( king_moves );
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
				add_if_legal_king( p, c, moves, 4 + row, 2 + row, move_flags::castle );
			}
		}
	}
	// Kingside castling
	if( p.castle[c] & 0x1 ) {
		if( p.get_occupancy( 0x60ull << row ) == 0 && !(possible_king_moves[6 + row] & other_kings ) ) {
			if( !detect_check( p, c, 5 + row, 5 + row ) ) {
				add_if_legal_king( p, c, moves, 4 + row, 6 + row, move_flags::castle );
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, move_info*& moves, check_map const& check, unsigned long long queen )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = rook_magic( queen, all_blockers ) | bishop_magic( queen, all_blockers );
	possible_moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]);

	while( possible_moves ) {
		unsigned long long queen_move = bitscan_unset( possible_moves );
		add_if_legal( moves, check, pieces::queen, queen, queen_move, move_flags::none );
	}
}


void calc_moves_queens( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long queens = p.bitboards[c].b[bb_type::queens];
	while( queens ) {
		unsigned long long queen = bitscan_unset( queens );
		calc_moves_queen( p, c, moves, check, queen );
	}
}


void calc_moves_bishop( position const& p, color::type c, move_info*& moves, check_map const& check,
					    unsigned long long bishop )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = bishop_magic( bishop, all_blockers );
	possible_moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]);

	while( possible_moves ) {
		unsigned long long bishop_move = bitscan_unset( possible_moves );
		add_if_legal( moves, check, pieces::bishop, bishop, bishop_move, move_flags::none );
	}
}


void calc_moves_bishops( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long bishops = p.bitboards[c].b[bb_type::bishops];
	while( bishops ) {
		unsigned long long bishop = bitscan_unset( bishops );
		calc_moves_bishop( p, c, moves, check, bishop );
	}
}


void calc_moves_rook( position const& p, color::type c, move_info*& moves, check_map const& check,
					  unsigned long long rook )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = rook_magic( rook, all_blockers );
	possible_moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]);

	while( possible_moves ) {
		unsigned long long rook_move = bitscan_unset( possible_moves);
		add_if_legal( moves, check, pieces::rook, rook, rook_move, move_flags::none );
	}
}


void calc_moves_rooks( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long rooks = p.bitboards[c].b[bb_type::rooks];
	while( rooks ) {
		unsigned long long rook = bitscan_unset( rooks );
		calc_moves_rook( p, c, moves, check, rook );
	}
}


void calc_moves_knight( position const& p, color::type c, move_info*& moves, check_map const& check,
						unsigned long long old_knight )
{
	unsigned long long new_knights = possible_knight_moves[old_knight] & ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]);
	while( new_knights ) {
		unsigned long long new_knight = bitscan_unset( new_knights );
		add_if_legal( moves, check, pieces::knight, old_knight, new_knight, move_flags::none );
	}
}


void calc_moves_knights( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long knights = p.bitboards[c].b[bb_type::knights];
	while( knights ) {
		unsigned long long knight = bitscan_unset( knights );
		calc_moves_knight( p, c, moves, check, knight );
	}
}


void calc_moves_pawn( position const& p, color::type c, move_info*& moves, check_map const& check,
					  unsigned long long pawn )
{
	unsigned char old_col = static_cast<unsigned char>(pawn % 8);
	unsigned char old_row = static_cast<unsigned char>(pawn / 8);

	unsigned char new_row = (c == color::white) ? (old_row + 1) : (old_row - 1);
	if( !p.is_occupied_square( old_col + new_row * 8 ) ) {
		unsigned char pawn_move = old_col + new_row * 8;
		if( new_row == 0 || new_row == 7 ) {
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::promotion_queen );
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::promotion_rook );
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::promotion_bishop );
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::promotion_knight );
		}
		else {
			add_if_legal( moves, check, pieces::pawn, pawn, pawn_move, move_flags::none );
		}

		if( old_row == ( (c == color::white) ? 1 : 6) ) {
			// Moving two rows from starting row
			new_row = (c == color::white) ? (old_row + 2) : (old_row - 2);

			if( !p.is_occupied_square( old_col + new_row * 8 ) ) {
				add_if_legal( moves, check, pieces::pawn, pawn, old_col + new_row * 8, move_flags::pawn_double_move );
			}
		}
	}
}

void calc_moves_pawns( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long pawns = p.bitboards[c].b[bb_type::pawns];
	while( pawns ) {
		unsigned long long pawn = bitscan_unset( pawns );
		calc_moves_pawn( p, c, moves, check, pawn );
	}
}
}

void calculate_moves_noncaptures( position const& p, color::type c, move_info*& moves, check_map const& check )
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
