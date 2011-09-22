#include "assert.hpp"
#include "moves.hpp"
#include "eval.hpp"
#include "util.hpp"
#include "calc.hpp"

#include <algorithm>
#include <iostream>
#include <string>

extern unsigned long long const possible_king_moves[];
extern unsigned long long const possible_knight_moves[];

namespace {

void do_add_move( position const& p, color::type c, int const current_evaluation, move_info*& moves, pieces2::type const& pi,
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
	mi.m.captured_piece = pieces2::none;
	mi.m.promotion = promotion;

	mi.evaluation = evaluate_move( p, c, current_evaluation, mi.m, mi.pawns );
}

// Adds the move if it does not result in self getting into check
void add_if_legal( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
				  pieces2::type const& pi2,
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

	do_add_move( p, c, current_evaluation, moves, pieces2::king, old_col, old_row, new_col, new_row, flags );
}

void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
					  unsigned char old_col, unsigned char old_row,
					  unsigned char new_col, unsigned char new_row )
{
	if( possible_king_moves[new_row * 8 + new_col] & p.bitboards[1-c].king ) {
		// Other king too close
		return;
	}

	unsigned char target = p.board2[new_col][new_row];
	if( !target ) {
		add_if_legal_king( p, c, current_evaluation, moves, old_col, old_row, new_col, new_row, move_flags::valid );
	}

}


void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long kings = p.bitboards[c].king;
	unsigned long long king;
	bitscan(kings, king);

	unsigned char old_col = static_cast<unsigned char>(king % 8);
	unsigned char old_row = static_cast<unsigned char>(king / 8);

	unsigned long long king_moves = possible_king_moves[king];
	unsigned long long i;
	while( king_moves ) {
		bitscan( king_moves, i );
		king_moves ^= 1ull << i;
		calc_moves_king( p, c, current_evaluation, moves, check,
						 old_col, old_row,
						 static_cast<unsigned char>(i % 8), static_cast<unsigned char>(i / 8) );
	}

	if( check.check ) {
		return;
	}

	// Queenside castling
	if( p.castle[c] & 0x2 ) {
		if( !p.board2[1][old_row] && !p.board2[2][old_row] && !p.board2[3][old_row] ) {
			if( !detect_check( p, c, 3, old_row, 3, old_row ) ) {
				add_if_legal_king( p, c, current_evaluation, moves, 4, old_row, 2, old_row, move_flags::valid | move_flags::castle );
			}
		}
	}
	// Kingside castling
	if( p.castle[c] & 0x1 ) {
		if( !p.board2[5][old_row] && !p.board2[6][old_row] ) {
			if( !detect_check( p, c, 5, old_row, 5, old_row ) ) {
				add_if_legal_king( p, c, current_evaluation, moves, 4, old_row, 6, old_row, move_flags::valid | move_flags::castle );
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, unsigned long long queen )
{
	for( int cx = -1; cx <= 1; ++cx ) {
		for( int cy = -1; cy <= 1; ++cy ) {
			if( !cx && !cy ) {
				continue;
			}

			int old_col = static_cast<int>(queen % 8);
			int old_row = static_cast<int>(queen / 8);

			int x, y;
			for( x = old_col + cx, y = old_row + cy; x >= 0 && x <= 7 && y >= 0 && y <= 7; x += cx, y += cy ) {
				unsigned char target = p.board2[x][y];
				if( !target ) {
					add_if_legal( p, c, current_evaluation, moves, check, pieces2::queen, old_col, old_row, x, y, move_flags::valid );
				}
				else {
					break;
				}
			}
		}
	}
}


void calc_moves_queens( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long queens = p.bitboards[c].queens;
	while( queens ) {
		unsigned long long queen;
		bitscan( queens, queen );	
		queens ^= 1ull << queen;
		calc_moves_queen( p, c, current_evaluation, moves, check, queen );
	}
}


void calc_moves_bishop( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
					    unsigned long long bishop )
{
	int old_col = static_cast<int>(bishop % 8);
	int old_row = static_cast<int>(bishop / 8);
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int cy = -1; cy <= 1; cy += 2 ) {
			int x, y;
			for( x = old_col + cx, y = old_row + cy; x >= 0 && x <= 7 && y >= 0 && y <= 7; x += cx, y += cy ) {
				unsigned char target = p.board2[x][y];
				if( !target ) {
					add_if_legal( p, c, current_evaluation, moves, check, pieces2::bishop, old_col, old_row, x, y, move_flags::valid );
				}
				else {
					break;
				}
			}
		}
	}
}


void calc_moves_bishops( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long bishops = p.bitboards[c].bishops;
	while( bishops ) {
		unsigned long long bishop;
		bitscan( bishops, bishop );	
		bishops ^= 1ull << bishop;
		calc_moves_bishop( p, c, current_evaluation, moves, check, bishop );
	}
}


void calc_moves_rook( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
					  unsigned long long rook )
{
	int old_col = static_cast<int>(rook % 8);
	int old_row = static_cast<int>(rook / 8);

	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int x = old_col + cx; x >= 0 && x <= 7; x += cx ) {
			unsigned char target = p.board2[x][old_row];
			if( !target ) {
				add_if_legal( p, c, current_evaluation, moves, check, pieces2::rook, old_col, old_row, x, old_row, move_flags::valid );
			}
			else {
				break;
			}
		}
	}
	for( int cy = -1; cy <= 1; cy += 2 ) {
		for( int y = old_row + cy; y >= 0 && y <= 7; y += cy ) {
			unsigned char target = p.board2[old_col][y];
			if( !target ) {
				add_if_legal( p, c, current_evaluation, moves, check, pieces2::rook, old_col, old_row, old_col, y, move_flags::valid );
			}
			else {
				break;
			}
		}
	}
}


void calc_moves_rooks( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long rooks = p.bitboards[c].rooks;
	while( rooks ) {
		unsigned long long rook;
		bitscan( rooks, rook );
		rooks ^= 1ull << rook;
		calc_moves_rook( p, c, current_evaluation, moves, check, rook );
	}
}


void calc_moves_knight( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
					    unsigned char old_col, unsigned char old_row,
						unsigned char new_col, unsigned char new_row )
{
	int target = p.board2[new_col][new_row];
	if( !target ) {
		add_if_legal( p, c, current_evaluation, moves, check, pieces2::knight, old_col, old_row, new_col, new_row, move_flags::valid );
	}
}

void calc_moves_knight( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check,
					    unsigned long long old_knight )
{
	unsigned char old_col = static_cast<unsigned char>(old_knight % 8);
	unsigned char old_row = static_cast<unsigned char>(old_knight / 8);

	unsigned long long new_knights = possible_knight_moves[old_knight];
	while( new_knights ) {
		unsigned long long new_knight;
		bitscan( new_knights, new_knight );
		new_knights ^= 1ull << new_knight;
		calc_moves_knight( p, c, current_evaluation, moves, check,
						   old_col, old_row,
						   static_cast<unsigned char>(new_knight % 8), static_cast<unsigned char>(new_knight / 8) );
	}
}


void calc_moves_knights( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long knights = p.bitboards[c].knights;
	while( knights ) {
		unsigned long long knight;
		bitscan( knights, knight );
		knights ^= 1ull << knight;
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
	
	unsigned char target = p.board2[old_col][new_row];
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
			add_if_legal( p, c, current_evaluation, moves, check, pieces2::pawn, old_col, old_row, old_col, new_row, flags );
		}
		
		if( old_row == ( (c == color::white) ? 1 : 6) ) {
			// Moving two rows from starting row
			new_row = (c == color::white) ? (old_row + 2) : (old_row - 2);

			unsigned char target = p.board2[old_col][new_row];
			if( !target ) {
				if( inverse_check.board[old_col][old_row] ||
					( (inverse_check.enemy_king_row == new_row + cy) &&
					  ((inverse_check.enemy_king_col == old_col + 1) || (inverse_check.enemy_king_col + 1 == old_col))
					) )
				{
					add_if_legal( p, c, current_evaluation, moves, check, pieces2::pawn, old_col, old_row, old_col, new_row, move_flags::valid );
				}
			}
		}
	}
}

void calc_moves_pawns( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	unsigned long long pawns = p.bitboards[c].pawns;
	while( pawns ) {
		unsigned long long pawn;
		bitscan( pawns, pawn );
		pawns ^= 1ull << pawn;

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
