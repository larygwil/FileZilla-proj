#include "assert.hpp"
#include "moves.hpp"
#include "eval.hpp"
#include "util.hpp"
#include "calc.hpp"
#include "sliding_piece_attacks.hpp"

#include <algorithm>
#include <iostream>
#include <string>

extern unsigned long long const possible_king_moves[];
extern unsigned long long const possible_knight_moves[];

namespace {

void do_add_move( position const& p, color::type c, int const current_evaluation, move_info*& moves, pieces::type const& pi,
				  unsigned char const& source, unsigned char const& target,
				  int flags, promotions::type promotion = promotions::queen )
{
	move_info& mi = *(moves++);

	mi.m.flags = flags;
	mi.m.piece = pi;
	mi.m.source = source;
	mi.m.target = target;
	mi.m.captured_piece = pieces::none;
	mi.m.promotion = promotion;

	mi.evaluation = evaluate_move( p, c, current_evaluation, mi.m, mi.pawns );
}

// Adds the move if it does not result in self getting into check
void add_if_legal( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
				  pieces::type const& pi,
				  unsigned char const& source, unsigned char target,
				  int flags, promotions::type promotion = promotions::queen )
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

	do_add_move( p, c, current_evaluation, moves, pi, source, target, flags, promotion );
}

void add_if_legal_king( position const& p, color::type c, int const current_evaluation, move_info*& moves,
						unsigned char const& source, unsigned char target,
					    int flags )
{
	if( detect_check( p, c, target, source ) ) {
		return;
	}

	do_add_move( p, c, current_evaluation, moves, pieces::king, source, target, flags );
}

void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves,
					  inverse_check_map const& inverse_check,
					  unsigned char source, unsigned char target )
{
	if( inverse_check.board[source] ) {
		add_if_legal_king( p, c, current_evaluation, moves, source, target, move_flags::valid );
	}
}


void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long kings = p.bitboards[c].b[bb_type::king];
	unsigned long long king;
	bitscan(kings, king);

	unsigned long long other_kings = p.bitboards[1-c].b[bb_type::king];
	unsigned long long other_king;
	bitscan(other_kings, other_king);

	unsigned long long king_moves = possible_king_moves[king] & ~(p.bitboards[c].b[bb_type::all_pieces] | possible_king_moves[other_king] | p.bitboards[1-c].b[bb_type::all_pieces]);
	while( king_moves ) {
		unsigned long long king_move;
		bitscan( king_moves, king_move );
		king_moves &= king_moves - 1;
		calc_moves_king( p, c, current_evaluation, moves,
						 inverse_check,
						 king, king_move );
	}

	if( check.check ) {
		return;
	}

	unsigned char row = c ? 56 : 0;
	// Queenside castling
	if( p.castle[c] & 0x2 ) {
		if( !p.board[1 + row] && !p.board[2 + row] && !p.board[3 + row] ) {
			if( !detect_check( p, c, 3 + row, 3 + row ) ) {
				add_if_legal_king( p, c, current_evaluation, moves, 4 + row, 2 + row, move_flags::valid | move_flags::castle );
			}
		}
	}
	// Kingside castling
	if( p.castle[c] & 0x1 ) {
		if( !p.board[5 + row] && !p.board[6 + row] ) {
			if( !detect_check( p, c, 5 + row, 5 + row ) ) {
				add_if_legal_king( p, c, current_evaluation, moves, 4 + row, 6 + row, move_flags::valid | move_flags::castle );
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
					   inverse_check_map const& inverse_check,
					   unsigned long long queen )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = rook_attacks( queen, all_blockers ) | bishop_attacks( queen, all_blockers );
	possible_moves &= ~all_blockers;

	while( possible_moves ) {
		unsigned long long queen_move;
		bitscan( possible_moves, queen_move );
		possible_moves &= possible_moves - 1;

		if( inverse_check.board[queen] || inverse_check.board[queen_move] ) {
			add_if_legal( p, c, current_evaluation, moves, check, pieces::queen, queen, queen_move, move_flags::valid );
		}
	}
}


void calc_moves_queens( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long queens = p.bitboards[c].b[bb_type::queens];
	while( queens ) {
		unsigned long long queen;
		bitscan( queens, queen );	
		queens &= queens - 1;
		calc_moves_queen( p, c, current_evaluation, moves, check, inverse_check, queen );
	}
}


void calc_moves_bishop( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
						inverse_check_map const& inverse_check,
					    unsigned long long bishop )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = bishop_attacks( bishop, all_blockers );
	possible_moves &= ~all_blockers;

	while( possible_moves ) {
		unsigned long long bishop_move;
		bitscan( possible_moves, bishop_move );
		possible_moves &= possible_moves - 1;

		if( inverse_check.board[bishop] || inverse_check.board[bishop_move] == 0xc0 ) {
			add_if_legal( p, c, current_evaluation, moves, check, pieces::bishop, bishop, bishop_move, move_flags::valid );
		}
	}
}


void calc_moves_bishops( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long bishops = p.bitboards[c].b[bb_type::bishops];
	while( bishops ) {
		unsigned long long bishop;
		bitscan( bishops, bishop );	
		bishops &= bishops - 1;
		calc_moves_bishop( p, c, current_evaluation, moves, check, inverse_check, bishop );
	}
}


void calc_moves_rook( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
					  inverse_check_map const& inverse_check,
					  unsigned long long rook )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = rook_attacks( rook, all_blockers );
	possible_moves &= ~all_blockers;

	while( possible_moves ) {
		unsigned long long rook_move;
		bitscan( possible_moves, rook_move );
		possible_moves &= possible_moves - 1;

		if( inverse_check.board[rook] || inverse_check.board[rook_move] == 0x80 ) {
			add_if_legal( p, c, current_evaluation, moves, check, pieces::rook, rook, rook_move, move_flags::valid );
		}
	}
}


void calc_moves_rooks( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long rooks = p.bitboards[c].b[bb_type::rooks];
	while( rooks ) {
		unsigned long long rook;
		bitscan( rooks, rook );
		rooks &= rooks - 1;
		calc_moves_rook( p, c, current_evaluation, moves, check, inverse_check, rook );
	}
}


void calc_moves_knight( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
						inverse_check_map const& inverse_check,
					    unsigned long long old_knight )
{
	unsigned long long new_knights = possible_knight_moves[old_knight] & ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]);

	unsigned long long other_kings = p.bitboards[1-c].b[bb_type::king];
	unsigned long long other_king;
	bitscan(other_kings, other_king);

	while( new_knights ) {
		unsigned long long new_knight;
		bitscan( new_knights, new_knight );
		new_knights &= new_knights - 1;
		if( new_knight & possible_knight_moves[other_king] || inverse_check.board[old_knight] ) {
			add_if_legal( p, c, current_evaluation, moves, check, pieces::knight, old_knight, new_knight, move_flags::valid );
		}
	}
}


void calc_moves_knights( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long knights = p.bitboards[c].b[bb_type::knights];
	while( knights ) {
		unsigned long long knight;
		bitscan( knights, knight );
		knights &= knights - 1;
		calc_moves_knight( p, c, current_evaluation, moves, check, inverse_check, knight );
	}
}


void calc_moves_pawn( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
					  inverse_check_map const& inverse_check,
					  unsigned long long pawn )
{
	unsigned char old_col = static_cast<unsigned char>(pawn % 8);
	unsigned char old_row = static_cast<unsigned char>(pawn / 8);

	signed char cy = (c == color::white) ? 1 : -1;
	unsigned char new_row = cy + old_row;
	
	unsigned char captured = p.board[old_col + new_row * 8];
	if( !captured ) {

		int flags = move_flags::valid;
		if( new_row == 0 || new_row == 7 ) {
			flags |= move_flags::promotion;
		}

		if( inverse_check.board[pawn] ||
			new_row == 0 || new_row == 7 ||
			( (inverse_check.enemy_king_row == new_row + cy) &&
			  ((inverse_check.enemy_king_col == old_col + 1) || (inverse_check.enemy_king_col + 1 == old_col))
			) )
		{
			add_if_legal( p, c, current_evaluation, moves, check, pieces::pawn, pawn, old_col + new_row * 8, flags );
		}
		
		if( old_row == ( (c == color::white) ? 1 : 6) ) {
			// Moving two rows from starting row
			new_row = (c == color::white) ? (old_row + 2) : (old_row - 2);

			unsigned char captured = p.board[old_col + new_row * 8];
			if( !captured ) {
				if( inverse_check.board[pawn] ||
					( (inverse_check.enemy_king_row == new_row + cy) &&
					  ((inverse_check.enemy_king_col == old_col + 1) || (inverse_check.enemy_king_col + 1 == old_col))
					) )
				{
					add_if_legal( p, c, current_evaluation, moves, check, pieces::pawn, pawn, old_col + new_row * 8, move_flags::valid | move_flags::pawn_double_move );
				}
			}
		}
	}
}

void calc_moves_pawns( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long pawns = p.bitboards[c].b[bb_type::pawns];
	while( pawns ) {
		unsigned long long pawn;
		bitscan( pawns, pawn );
		pawns &= pawns - 1;

		calc_moves_pawn( p, c, current_evaluation, moves, check, inverse_check, pawn );
	}
}
}

void calculate_moves_checks( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	calc_moves_king( p, c, current_evaluation, moves, check, inverse_check );

	calc_moves_pawns( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_queens( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_rooks( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_bishops( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_knights( p, c, current_evaluation, moves, check, inverse_check );
}
