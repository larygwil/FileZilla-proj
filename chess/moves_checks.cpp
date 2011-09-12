#include "assert.hpp"
#include "moves.hpp"
#include "eval.hpp"
#include "util.hpp"

#include <algorithm>
#include <iostream>
#include <string>

extern unsigned long long const possible_king_moves[];
extern unsigned long long const possible_knight_moves[];

namespace {

// Adds the move if it does not result in a check
void add_if_legal( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, unsigned char const& pi, unsigned char const& new_col, unsigned char const& new_row )
{
	piece const& pp = p.pieces[c][pi];
	unsigned char const& cv_old = check.board[pp.column][pp.row];
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

	move_info mi;

	mi.m.source_col = pp.column;
	mi.m.source_row = pp.row;
	mi.m.target_col = new_col;
	mi.m.target_row = new_row;

	mi.evaluation = evaluate_move( p, c, current_evaluation, mi.m, mi.pawns );

	*(moves++) = mi;
}

void add_if_legal_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, unsigned char new_col, unsigned char new_row )
{
	piece const& kp = p.pieces[c][pieces::king];
	if( detect_check( p, c, new_col, new_row, kp.column, kp.row ) ) {
		return;
	}

	move_info mi;
	mi.m.source_col = kp.column;
	mi.m.source_row = kp.row;
	mi.m.target_col = new_col;
	mi.m.target_row = new_row;

	mi.evaluation = evaluate_move( p, c, current_evaluation, mi.m, mi.pawns );

	*(moves++) = mi;
}

void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check, piece const& pp, unsigned char new_col, unsigned char new_row )
{
	piece const& other_king = p.pieces[1-c][pieces::king];
	if( possible_king_moves[new_row * 8 + new_col] & (1ull << (other_king.column + other_king.row * 8)) ) {
		// Other king too close
		return;
	}

	unsigned char target_piece = p.board[new_col][new_row];
	if( target_piece != pieces::nil ) {
		return;
	}
	else if( !inverse_check.board[pp.column][pp.row] ) {
		return;
	}

	add_if_legal_king( p, c, current_evaluation, moves, new_col, new_row );
}


void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	piece const& pp = p.pieces[c][pieces::king];

	unsigned long long kings = possible_king_moves[pp.column + pp.row * 8];
	unsigned long long i;
	while( kings ) {
		bitscan( kings, i );
		kings ^= 1ull << i;
		calc_moves_king( p, c, current_evaluation, moves, check, inverse_check, pp, i & 0x7, i >> 3 );
	}

	// Queenside castling
	if( p.pieces[c][pieces::rook1].special ) {
		if( inverse_check.board[3][pp.row] || inverse_check.board[4][pp.row]) {
			if( p.board[1][pp.row] == pieces::nil && p.board[2][pp.row] == pieces::nil && p.board[3][pp.row] == pieces::nil ) {
				if( !detect_check( p, c, 3, pp.row, 3, pp.row ) ) {
					add_if_legal_king( p, c, current_evaluation, moves, 2, pp.row );
				}
			}
		}
	}
	// Kingside castling
	if( p.pieces[c][pieces::rook2].special ) {
		if( inverse_check.board[5][pp.row] || inverse_check.board[4][pp.row]) {
			if( p.board[5][pp.row] == pieces::nil && p.board[6][pp.row] == pieces::nil ) {
				if( !detect_check( p, c, 5, pp.row, 5, pp.row ) ) {
					add_if_legal_king( p, c, current_evaluation, moves, 6, pp.row );
				}
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; ++cx ) {
		for( int cy = -1; cy <= 1; ++cy ) {
			if( !cx && !cy ) {
				continue;
			}

			int x, y;
			for( x = pp.column + cx, y = pp.row + cy; x >= 0 && x <= 7 && y >= 0 && y <= 7; x += cx, y += cy ) {
				unsigned char target = p.board[x][y];
				if( target != pieces::nil ) {
					break;
				}
				if( inverse_check.board[pp.column][pp.row] || inverse_check.board[x][y] ) {
					add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
				}
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	piece const& pp = p.pieces[c][pieces::queen];
	if( pp.alive ) {
		calc_moves_queen( p, c, current_evaluation, moves, check, inverse_check, pieces::queen, pp );
	}
}

void calc_moves_bishop( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int cy = -1; cy <= 1; cy += 2 ) {
			int x, y;
			for( x = pp.column + cx, y = pp.row + cy; x >= 0 && x <= 7 && y >= 0 && y <= 7; x += cx, y += cy ) {
				unsigned char target = p.board[x][y];
				if( target != pieces::nil ) {
					break;
				}
				if( inverse_check.board[pp.column][pp.row] ||
					inverse_check.board[x][y] == 0xc0 )
				{
					add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
				}
			}
		}
	}
}


void calc_moves_bishops( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	{
		piece const& pp = p.pieces[c][pieces::bishop1];
		if( pp.alive ) {
			calc_moves_bishop( p, c, current_evaluation, moves, check, inverse_check, pieces::bishop1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::bishop2];
		if( pp.alive ) {
			calc_moves_bishop( p, c, current_evaluation, moves, check, inverse_check, pieces::bishop2, pp );
		}
	}
}


void calc_moves_rook( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int x = pp.column + cx; x >= 0 && x <= 7; x += cx ) {
			unsigned char target = p.board[x][pp.row];
			if( target != pieces::nil ) {
				break;
			}
			if( inverse_check.board[pp.column][pp.row] || inverse_check.board[x][pp.row] == 0x80 ) {
				add_if_legal( p, c, current_evaluation, moves, check, pi, x, pp.row );
			}
		}
	}
	for( int cy = -1; cy <= 1; cy += 2 ) {
		for( int y = pp.row + cy; y >= 0 && y <= 7; y += cy ) {
			unsigned char target = p.board[pp.column][y];
			if( target != pieces::nil ) {
				break;
			}
			if( inverse_check.board[pp.column][pp.row] || inverse_check.board[pp.column][y] == 0x80 ) {
				add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, y );
			}
		}
	}
}


void calc_moves_rooks( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	{
		piece const& pp = p.pieces[c][pieces::rook1];
		if( pp.alive ) {
			calc_moves_rook( p, c, current_evaluation, moves, check, inverse_check, pieces::rook1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::rook2];
		if( pp.alive ) {
			calc_moves_rook( p, c, current_evaluation, moves, check, inverse_check, pieces::rook2, pp );
		}
	}
}


void calc_moves_knight( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check, pieces::type pi, piece const& pp, int new_column, int new_row )
{
	int new_target = p.board[new_column][new_row];
	if( new_target != pieces::nil ) {
		return;
	}

	if( !inverse_check.board[pp.column][pp.row] ) {
		unsigned long long mask = 1ull << (p.pieces[1-c][pieces::king].column + p.pieces[1-c][pieces::king].row * 8);
		if( !(possible_knight_moves[new_column + new_row * 8] & mask ) ) {
			return;
		}
	}

	add_if_legal( p, c, current_evaluation, moves, check, pi, new_column, new_row );
}


void calc_moves_knight( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check, pieces::type pi, piece const& pp )
{
	unsigned long long knights = possible_knight_moves[pp.column + pp.row * 8];
	unsigned long long knight;
	while( knights ) {
		bitscan( knights, knight );
		knights ^= 1ull << knight;
		calc_moves_knight( p, c, current_evaluation, moves, check, inverse_check, pi, pp, knight & 0x7, knight >> 3 );
	}
}


void calc_moves_knights( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	{
		piece const& pp = p.pieces[c][pieces::knight1];
		if( pp.alive ) {
			calc_moves_knight( p, c, current_evaluation, moves, check, inverse_check, pieces::knight1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::knight2];
		if( pp.alive ) {
			calc_moves_knight( p, c, current_evaluation, moves, check, inverse_check, pieces::knight2, pp );
		}
	}
}

void calc_moves_pawns( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	for( unsigned int pi = pieces::pawn1; pi <= pieces::pawn8; ++pi ) {
		piece const& pp = p.pieces[c][pi];
		if( pp.alive ) {
			if( !pp.special ) {
				signed char cy = (c == color::white) ? 1 : -1;
				unsigned char new_row = cy + pp.row;
				unsigned char target = p.board[pp.column][new_row];

				if( target != pieces::nil ) {
					continue;
				}

				unsigned char king_col = p.pieces[1-c][pieces::king].column;
				unsigned char king_row = p.pieces[1-c][pieces::king].row;
				if( inverse_check.board[pp.column][pp.row] ) {
					add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, new_row );
				}
				else if( new_row == 0 || new_row == 7 ) {
					add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, new_row );
				}
				else if( (king_row == new_row + cy) &&
					  ((king_col == pp.column + 1) || (king_col + 1 == pp.column))
					)
				{
					add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, new_row );
				}

				if( pp.row == ( (c == color::white) ? 1 : 6) ) {
					// Moving two rows from starting row
					new_row = (c == color::white) ? (pp.row + 2) : (pp.row - 2);

					unsigned char target = p.board[pp.column][new_row];
					if( target == pieces::nil ) {
						if( inverse_check.board[pp.column][pp.row] ||
							( (king_row == new_row + cy) &&
							  ((king_col == pp.column + 1) || (king_col + 1 == pp.column))
							))
						{
							add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, new_row );
						}
					}
				}
			}
			else {
				// Promoted piece
				unsigned char promoted = (p.promotions[c] >> (2 * (pi - pieces::pawn1) ) ) & 0x03;
				if( promoted == promotions::queen ) {
					calc_moves_queen( p, c, current_evaluation, moves, check, inverse_check, static_cast<pieces::type>(pi), pp );
				}
				else if( promoted == promotions::rook ) {
					calc_moves_rook( p, c, current_evaluation, moves, check, inverse_check, static_cast<pieces::type>(pi), pp );
				}
				else if( promoted == promotions::bishop ) {
					calc_moves_bishop( p, c, current_evaluation, moves, check, inverse_check, static_cast<pieces::type>(pi), pp );
				}
				else {//if( promoted == promotions::knight ) {
					calc_moves_knight( p, c, current_evaluation, moves, check, inverse_check, static_cast<pieces::type>(pi), pp );
				}
			}
		}
	}
}
}

void calculate_moves_checks( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	calc_moves_king( p, c, current_evaluation, moves, check, inverse_check );

	calc_moves_pawns( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_queen( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_rooks( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_bishops( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_knights( p, c, current_evaluation, moves, check, inverse_check );
}
