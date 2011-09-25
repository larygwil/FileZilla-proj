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
				  unsigned char const& old_col, unsigned char const& old_row,
				  unsigned char const& new_col, unsigned char const& new_row,
				  int flags, promotions::type promotion = promotions::queen )
{
	move_info& mi = *(moves++);

	mi.m.flags = flags;
	mi.m.piece = pi;
	mi.m.source_col = old_col;
	mi.m.source_row = old_row;
	mi.m.target_col = new_col;
	mi.m.target_row = new_row;
	mi.m.captured_piece = pieces::none;
	mi.m.promotion = promotion;

	mi.evaluation = evaluate_move( p, c, current_evaluation, mi.m, mi.pawns );
}

// Adds the move if it does not result in self getting into check
void add_if_legal( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
				  pieces::type const& pi2,
				  unsigned char const& old_col, unsigned char const& old_row,
				  unsigned char const& new_col, unsigned char const& new_row,
				  int flags, promotions::type promotion = promotions::queen )
{
	unsigned char const& cv_old = check.board[old_col][old_row];
	unsigned char const& cv_new = check.board[new_col][new_row];
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

	do_add_move( p, c, current_evaluation, moves, pi2, old_col, old_row, new_col, new_row, flags, promotion );
}

void add_if_legal_king( position const& p, color::type c, int const current_evaluation, move_info*& moves,
					    unsigned char const& old_col, unsigned char const& old_row,
					    unsigned char new_col, unsigned char new_row,
					    int flags )
{
	if( detect_check( p, c, new_col, new_row, old_col, old_row ) ) {
		return;
	}

	do_add_move( p, c, current_evaluation, moves, pieces::king, old_col, old_row, new_col, new_row, flags );
}

void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
					  inverse_check_map const& inverse_check,
					  unsigned char old_col, unsigned char old_row,
					  unsigned char new_col, unsigned char new_row )
{
	if( inverse_check.board[old_col][old_row] ) {
		add_if_legal_king( p, c, current_evaluation, moves, old_col, old_row, new_col, new_row, move_flags::valid );
	}
}


void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long kings = p.bitboards[c].b[bb_type::king];
	unsigned long long king;
	bitscan(kings, king);

	unsigned char old_col = static_cast<unsigned char>(king % 8);
	unsigned char old_row = static_cast<unsigned char>(king / 8);

	unsigned long long other_kings = p.bitboards[1-c].b[bb_type::king];
	unsigned long long other_king;
	bitscan(other_kings, other_king);

	unsigned long long king_moves = possible_king_moves[king] & ~(p.bitboards[c].b[bb_type::all_pieces] | possible_king_moves[other_king] | p.bitboards[1-c].b[bb_type::all_pieces]);
	while( king_moves ) {
		unsigned long long i;
		bitscan( king_moves, i );
		king_moves &= king_moves - 1;
		calc_moves_king( p, c, current_evaluation, moves, check,
						 inverse_check,
						 old_col, old_row,
						 static_cast<unsigned char>(i % 8), static_cast<unsigned char>(i / 8) );
	}

	if( check.check ) {
		return;
	}

	// Queenside castling
	if( p.castle[c] & 0x2 ) {
		if( !p.board[1][old_row] && !p.board[2][old_row] && !p.board[3][old_row] ) {
			if( !detect_check( p, c, 3, old_row, 3, old_row ) ) {
				add_if_legal_king( p, c, current_evaluation, moves, 4, old_row, 2, old_row, move_flags::valid | move_flags::castle );
			}
		}
	}
	// Kingside castling
	if( p.castle[c] & 0x1 ) {
		if( !p.board[5][old_row] && !p.board[6][old_row] ) {
			if( !detect_check( p, c, 5, old_row, 5, old_row ) ) {
				add_if_legal_king( p, c, current_evaluation, moves, 4, old_row, 6, old_row, move_flags::valid | move_flags::castle );
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

	unsigned char old_col = static_cast<unsigned char>(queen % 8);
	unsigned char old_row = static_cast<unsigned char>(queen / 8);

	unsigned long long queen_move;
	while( possible_moves ) {
		bitscan( possible_moves, queen_move );
		possible_moves &= possible_moves - 1;

		unsigned char new_col = static_cast<unsigned char>(queen_move % 8);
		unsigned char new_row = static_cast<unsigned char>(queen_move / 8);

		if( inverse_check.board[old_col][old_row] || inverse_check.board[new_col][new_row] ) {
			add_if_legal( p, c, current_evaluation, moves, check, pieces::queen, old_col, old_row, new_col, new_row, move_flags::valid );
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

	unsigned char old_col = static_cast<unsigned char>(bishop % 8);
	unsigned char old_row = static_cast<unsigned char>(bishop / 8);

	unsigned long long bishop_move;
	while( possible_moves ) {
		bitscan( possible_moves, bishop_move );
		possible_moves &= possible_moves - 1;

		unsigned char new_col = static_cast<unsigned char>(bishop_move % 8);
		unsigned char new_row = static_cast<unsigned char>(bishop_move / 8);

		if( inverse_check.board[old_col][old_row] || inverse_check.board[new_col][new_row] == 0xc0 ) {
			add_if_legal( p, c, current_evaluation, moves, check, pieces::bishop, old_col, old_row, new_col, new_row, move_flags::valid );
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

	unsigned char old_col = static_cast<unsigned char>(rook % 8);
	unsigned char old_row = static_cast<unsigned char>(rook / 8);

	unsigned long long rook_move;
	while( possible_moves ) {
		bitscan( possible_moves, rook_move );
		possible_moves &= possible_moves - 1;

		unsigned char new_col = static_cast<unsigned char>(rook_move % 8);
		unsigned char new_row = static_cast<unsigned char>(rook_move / 8);

		if( inverse_check.board[old_col][old_row] || inverse_check.board[new_col][new_row] == 0x80 ) {
			add_if_legal( p, c, current_evaluation, moves, check, pieces::rook, old_col, old_row, new_col, new_row, move_flags::valid );
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
					    unsigned char old_col, unsigned char old_row,
						unsigned char new_col, unsigned char new_row )
{
	add_if_legal( p, c, current_evaluation, moves, check, pieces::knight, old_col, old_row, new_col, new_row, move_flags::valid );
}

void calc_moves_knight( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
					    unsigned long long old_knight )
{
	unsigned char old_col = static_cast<unsigned char>(old_knight % 8);
	unsigned char old_row = static_cast<unsigned char>(old_knight / 8);

	unsigned long long new_knights = possible_knight_moves[old_knight] & ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]);

	unsigned long long other_kings = p.bitboards[1-c].b[bb_type::king];
	unsigned long long other_king;
	bitscan(other_kings, other_king);
	new_knights &= possible_knight_moves[other_king];

	while( new_knights ) {
		unsigned long long new_knight;
		bitscan( new_knights, new_knight );
		new_knights &= new_knights - 1;
		calc_moves_knight( p, c, current_evaluation, moves, check,
						   old_col, old_row,
						   static_cast<unsigned char>(new_knight % 8), static_cast<unsigned char>(new_knight / 8) );
	}
}


void calc_moves_knights( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long knights = p.bitboards[c].b[bb_type::knights];
	while( knights ) {
		unsigned long long knight;
		bitscan( knights, knight );
		knights &= knights - 1;
		calc_moves_knight( p, c, current_evaluation, moves, check, knight );
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
	
	unsigned char target = p.board[old_col][new_row];
	if( !target ) {

		int flags = move_flags::valid;
		if( new_row == 0 || new_row == 7 ) {
			flags |= move_flags::promotion;
		}

		if( inverse_check.board[old_col][old_row] ||
			new_row == 0 || new_row == 7 ||
			( (inverse_check.enemy_king_row == new_row + cy) &&
			  ((inverse_check.enemy_king_col == old_col + 1) || (inverse_check.enemy_king_col + 1 == old_col))
			) )
		{
			add_if_legal( p, c, current_evaluation, moves, check, pieces::pawn, old_col, old_row, old_col, new_row, flags );
		}
		
		if( old_row == ( (c == color::white) ? 1 : 6) ) {
			// Moving two rows from starting row
			new_row = (c == color::white) ? (old_row + 2) : (old_row - 2);

			unsigned char target = p.board[old_col][new_row];
			if( !target ) {
				if( inverse_check.board[old_col][old_row] ||
					( (inverse_check.enemy_king_row == new_row + cy) &&
					  ((inverse_check.enemy_king_col == old_col + 1) || (inverse_check.enemy_king_col + 1 == old_col))
					) )
				{
					add_if_legal( p, c, current_evaluation, moves, check, pieces::pawn, old_col, old_row, old_col, new_row, move_flags::valid );
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
