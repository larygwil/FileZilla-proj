/*
Octochess
---------

Copyright (C) 2011 Tim "codesquid" Kosse
http://filezilla-project.org/

Distributed under the terms and conditions of the GNU General Public License v2+.



Ideas for optimizations:
- Factor depths into evaluation. Save evaluation different depths -> prefer the earlier position
- Transposition tables
- Iterative deepening

*/

#include "chess.hpp"

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <assert.h>

#include <windows.h>

#ifdef USE_STATISTICS
namespace statistics {
struct type {
	unsigned long long evaluated_leaves;
	unsigned long long evaluated_intermediate;
#if USE_QUIESCENCE
	unsigned long long quiescence_moves;
#endif
};
}

statistics::type stats = {0};
#endif

//#define ASSERT(x) assert(x);


#define ASSERT(x) do{ \
	if( !(x) ) { \
	std::cerr << "Assertion failed: " << #x << std::endl; \
	for(;;){}\
		abort(); \
	} \
	break; \
} while( true );
#undef ASSERT
#ifndef ASSERT
#define ASSERT(x)
#endif

void init_board( position& p )
{
	for( unsigned int c = 0; c <= 1; ++c ) {
		for( unsigned int i = 0; i < 8; ++i ) {
			p.pieces[c][pieces::pawn1 + i].column = i;
			p.pieces[c][pieces::pawn1 + i].row = (c == color::black) ? 6 : 1;

			p.pieces[c][pieces::pawn8 + 1 + i].row = (c == color::black) ? 7 : 0;
		}

		p.pieces[c][pieces::rook1].column = 0;
		p.pieces[c][pieces::rook2].column = 7;
		p.pieces[c][pieces::bishop1].column = 2;
		p.pieces[c][pieces::bishop2].column = 5;
		p.pieces[c][pieces::knight1].column = 1;
		p.pieces[c][pieces::knight2].column = 6;
		p.pieces[c][pieces::king].column = 4;
		p.pieces[c][pieces::queen].column = 3;
	}

	for( unsigned int x = 0; x < 8; ++x ) {
		for( unsigned int y = 0; y < 8; ++y ) {
			p.board[x][y] = pieces::nil;
		}
	}

	for( unsigned int c = 0; c <= 1; ++c ) {
		for( unsigned int i = 0; i < 16; ++i ) {
			p.pieces[c][i].alive = true;
			p.pieces[c][i].special = false;

			p.board[p.pieces[c][i].column][p.pieces[c][i].row] = i | (c << 4);
		}

		p.pieces[c][pieces::rook1].special = true;
		p.pieces[c][pieces::rook2].special = true;
	}

	p.promotions[0] = 0;
	p.promotions[1] = 0;

	p.can_en_passant = pieces::nil;
}

void init_board_from_pieces( position& p )
{
	for( unsigned int x = 0; x < 8; ++x ) {
		for( unsigned int y = 0; y < 8; ++y ) {
			p.board[x][y] = pieces::nil;
		}
	}

	for( unsigned int c = 0; c <= 1; ++c ) {
		for( unsigned int i = 0; i < 16; ++i ) {
			if( p.pieces[c][i].alive ) {
				p.board[p.pieces[c][i].column][p.pieces[c][i].row] = i | (c << 4);
			}
		}
	}
}

// Returns false on multiple, further check testing is skipped
bool detect_check_knight( position const& p, color::type c, int column, int row, int cx, int cy, check_info& check )
{
	column += cx;
	if( column < 0 || column > 7 ) {
		return true;
	}

	row += cy;
	if( row < 0 || row > 7 ) {
		return true;
	}

	int pi = p.board[column][row];
	if( pi == pieces::nil ) {
		return true;
	}

	if( c != (pi >> 4) ) {
		return true;
	}

	pi &= 0x0f;

	if( pi == pieces::knight1 || pi == pieces::knight2 ) {
		if( check.check ) {
			check.multiple = true;
			return false;
		}
		check.check = true;
		check.piece = pi;
	}
	else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
		piece const& pp = p.pieces[c][pi];
		if( pp.special ) {
			unsigned short pknight = promotions::knight << (2 * (pi - pieces::pawn1) );
			if( (p.promotions[c] & pknight) == pknight ) {
				if( check.check ) {
					check.multiple = true;
					return false;
				}
				check.check = true;
				check.piece = pi;
			}
		}
	}

	return true;
}

#define SET_CHECK_RET_ON_MULTIPLE(check, pi) { if( check.check ) { check.multiple = true; return; } check.check = true; check.piece = pi; }
void detect_check( position const& p, color::type c, check_info& check, int king_col, int king_row )
{
	check.check = false;
	check.multiple = false;
	check.knight = false;

	// Check diagonals
	int col, row;
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int cy = -1; cy <= 1; cy += 2 ) {
			for( col = king_col + cx, row = king_row + cy;
				   col >= 0 && col < 8 && row >= 0 && row < 8; col += cx, row += cy ) {
				
				unsigned char index = p.board[col][row];
				if( index == pieces::nil ) {
					continue;
				}

				unsigned char piece_color = (index >> 4) & 0x1;
				if( piece_color == c )
					break; // Own pieces do not check.

				unsigned char pi = index & 0x0f;

				if( pi == pieces::queen || pi == pieces::bishop1 || pi == pieces::bishop2 ) {
					SET_CHECK_RET_ON_MULTIPLE( check, pi );
				}
				else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
					// Check for promoted queens
					piece const& pp = p.pieces[1 - c][pi];
					if( pp.special ) {
						unsigned short promoted = p.promotions[1 - c] >> ((pi - pieces::pawn1) * 2);
						if( promoted == promotions::queen || promoted == promotions::bishop ) {
							SET_CHECK_RET_ON_MULTIPLE( check, pi )
						}
					}
					else if( c && king_row == (row + 1) ) {
						// The pawn itself giving chess
						SET_CHECK_RET_ON_MULTIPLE( check, pi )
					}
					else if( !c && row == (king_row + 1) ) {
						// The pawn itself giving chess
						SET_CHECK_RET_ON_MULTIPLE( check, pi )
					}
				}

				break;
			}
		}
	}

	// Horizontals
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( col = king_col + cx;
			col >= 0 && col < 8; col += cx )
		{
			unsigned char index = p.board[col][king_row];
			if( index == pieces::nil ) {
				continue;
			}

			unsigned char piece_color = (index >> 4) & 0x1;
			if( piece_color == c )
				break; // Own pieces do not check.

			unsigned char pi = index & 0x0f;

			if( pi == pieces::queen || pi == pieces::rook1 || pi == pieces::rook2 ) {
				SET_CHECK_RET_ON_MULTIPLE( check, pi )
			}
			else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
				// Check for promoted queen and rooks
				piece const& pp = p.pieces[1 - c][pi];
				if( pp.special ) {
					unsigned short promoted = p.promotions[1 - c] >> ((pi - pieces::pawn1) * 2);
					if( promoted == promotions::queen || promoted == promotions::rook ) {
						SET_CHECK_RET_ON_MULTIPLE( check, pi )
					}
				}
			}

			break;
		}
	}

	// Verticals
	for( int cy = -1; cy <= 1; cy += 2 ) {
		for( row = king_row + cy;
			row >= 0 && row < 8; row += cy )
		{
			unsigned char index = p.board[king_col][row];
			if( index == pieces::nil ) {
				continue;
			}

			unsigned char piece_color = (index >> 4) & 0x1;
			if( piece_color == c )
				break; // Own pieces do not check.

			unsigned char pi = index & 0x0f;
			
			if( pi == pieces::queen || pi == pieces::rook1 || pi == pieces::rook2 ) {
				SET_CHECK_RET_ON_MULTIPLE( check, pi )
			}
			else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
				// Check for promoted queen and rooks
				piece const& pp = p.pieces[1 - c][pi];
				if( pp.special ) {
					unsigned short promoted = p.promotions[1 - c] >> ((pi - pieces::pawn1) * 2);
					if( promoted == promotions::queen || promoted == promotions::rook ) {
						SET_CHECK_RET_ON_MULTIPLE( check, pi )
					}
				}
			}

			break;
		}
	}

	if( !detect_check_knight( p, static_cast<color::type>(1-c), king_col, king_row, 1, 2, check ) ) return;
	if( !detect_check_knight( p, static_cast<color::type>(1-c), king_col, king_row, 2, 1, check ) ) return;
	if( !detect_check_knight( p, static_cast<color::type>(1-c), king_col, king_row, 2, -1, check ) ) return;
	if( !detect_check_knight( p, static_cast<color::type>(1-c), king_col, king_row, 1, -2, check ) ) return;
	if( !detect_check_knight( p, static_cast<color::type>(1-c), king_col, king_row, -1, -2, check ) ) return;
	if( !detect_check_knight( p, static_cast<color::type>(1-c), king_col, king_row, -2, -1, check ) ) return;
	if( !detect_check_knight( p, static_cast<color::type>(1-c), king_col, king_row, -2, 1, check ) ) return;
	if( !detect_check_knight( p, static_cast<color::type>(1-c), king_col, king_row, -1, 2, check ) ) return;
}


void detect_check( position const& p, color::type c, check_info& check )
{
	unsigned char column = p.pieces[c][pieces::king].column;
	unsigned char row = p.pieces[c][pieces::king].row;

	return detect_check( p, c, check, column, row );
}


int evaluate_side( color::type c, position const& p, check_info const& check )
{
	int result = 0;

	// To start: Count material in centipawns
	for( unsigned int i = 0; i < 16; ++i) {
		piece const& pp = p.pieces[c][i];
		if( pp.alive ) {
			if (pp.special && i >= pieces::pawn1 && i <= pieces::pawn8) {
				// Promoted pawn
				unsigned short promoted = (p.promotions[c] >> ((i - pieces::pawn1) * 2)) & 0x03;
				result += promotion_values[promoted];
			}
			else {
				result += piece_values[i];
			}
		}
	}

	for( int i = 0; i < 2; ++i ) {
		piece const& pp = p.pieces[c][pieces::knight1 + 1];
		if( pp.alive ) {
			if( pp.column == 0 || pp.column == 7 ) {
				result -= special_values::knight_at_border;
			}
			
			if( pp.row == 0 || pp.row == 7 ) {
				result -= special_values::knight_at_border;
			}
		}
	}

	if( p.pieces[c][pieces::king].special ) {
		result += special_values::castled;
	}

	return result;
}

int evaluate( color::type c, position const& p, check_info const& check )
{
	int result = evaluate_side( c, p, check ) - evaluate_side( static_cast<color::type>(1-c), p, check );

	return result;
}


bool apply_move( position& p, move const& m, color::type c )
{
	bool ret = false;

	piece& pp = p.pieces[c][m.piece];

	ASSERT( pp.alive );

	p.board[pp.column][pp.row] = pieces::nil;

	// Handle castling
	if( m.piece == pieces::king ) {
		if( pp.column == 4 ) {
			if( m.target_col == 6 ) {
				// Kingside
				ASSERT( !pp.special );
				ASSERT( p.pieces[c][pieces::rook2].special );
				ASSERT( p.pieces[c][pieces::rook2].alive );
				pp.special = 1;
				pp.column = 6;
				p.board[m.target_col][m.target_row] = pieces::rook2 | (c << 4);
				p.board[7][m.target_row] = pieces::nil;
				p.board[5][m.target_row] = pieces::rook2;
				p.pieces[c][pieces::rook2].column = 5;
				p.pieces[c][pieces::rook1].special = 0;
				p.pieces[c][pieces::rook2].special = 0;
				p.can_en_passant = pieces::nil;
				return false;
			}
			else if( m.target_col == 2 ) {
				// Queenside
				pp.special = 1;
				pp.column = 2;
				p.board[m.target_col][m.target_row] = pieces::rook1 | (c << 4);
				p.board[0][m.target_row] = pieces::nil;
				p.board[3][m.target_row] = pieces::rook1;
				p.pieces[c][pieces::rook1].column = 3;
				p.pieces[c][pieces::rook1].special = 0;
				p.pieces[c][pieces::rook2].special = 0;
				p.can_en_passant = pieces::nil;
				return false;
			}
		}
		p.pieces[c][pieces::rook1].special = 0;
		p.pieces[c][pieces::rook2].special = 0;
	}
	else if( m.piece == pieces::rook1 ) {
		pp.special = 0;
	}
	else if( m.piece == pieces::rook2 ) {
		pp.special = 0;
	}

	bool const is_pawn = m.piece >= pieces::pawn1 && m.piece <= pieces::pawn8 && !pp.special;

	unsigned char old_piece = p.board[m.target_col][m.target_row];
	if( old_piece != pieces::nil ) {
		// Ordinary capture
		ASSERT( (old_piece >> 4) != c );
		old_piece &= 0x0f;
		ASSERT( old_piece != pieces::king );
		p.pieces[1-c][old_piece].alive = false;
		p.pieces[1-c][old_piece].special = false;

		ret = true;
	}
	else if( is_pawn && p.pieces[c][m.piece].column != m.target_col )
	{
		// Must be en-passant
		old_piece = p.board[m.target_col][pp.row];
		ASSERT( (old_piece >> 4) != c );
		old_piece &= 0x0f;
		ASSERT( p.can_en_passant == old_piece );
		ASSERT( old_piece >= pieces::pawn1 && old_piece <= pieces::pawn8 );
		ASSERT( old_piece != pieces::king );
		p.pieces[1-c][old_piece].alive = false;
		p.pieces[1-c][old_piece].special = false;
		p.board[m.target_col][pp.row] = pieces::nil;

		ret = true;
	}

	if( is_pawn ) {
		if( m.target_row == 0 || m.target_row == 7) {
			pp.special = true;
			p.promotions[c] |= promotions::queen << (2 * (m.piece - pieces::pawn1) );
			p.can_en_passant = pieces::nil;
		}
		else if( (c == color::white) ? (pp.row + 2 == m.target_row) : (m.target_row + 2 == pp.row) ) {
			p.can_en_passant = m.piece;
		}
		else {
			p.can_en_passant = pieces::nil;
		}
	}
	else {
		p.can_en_passant = pieces::nil;
	}

	p.board[m.target_col][m.target_row] = m.piece | (c << 4);

	pp.column = m.target_col;
	pp.row = m.target_row;

	return ret;
}

// Adds the move if it does not result in a check
void add_if_legal( position const& p, color::type c, std::vector<move>& moves, check_info& check, unsigned char pi, unsigned char new_col, unsigned char new_row, unsigned char priority = 0 )
{
	position p2 = p;
	move m;
	m.piece = pi;
	m.target_col = new_col;
	m.target_row = new_row;
	
	apply_move( p2, m, c );

	check_info new_check;
	detect_check( p2, c, new_check );

	if( new_check.check ) {
		return;
	}

	m.priority = priority | (rand() % 0x0f);

	moves.push_back(m);
}

void calc_moves_king( position const& p, color::type c, std::vector<move>& moves, check_info& check, signed char cx, signed char cy )
{
	piece const& pp = p.pieces[c][pieces::king];
	if( cx < 0 && pp.column < 1 ) {
		return;
	}
	if( cy < 0 && pp.row < 1 ) {
		return;
	}
	if( cx > 0 && pp.column >= 7 ) {
		return;
	}
	if( cy > 0 && pp.row >= 7 ) {
		return;
	}

	unsigned char new_col = pp.column + cx;
	unsigned char new_row = pp.row + cy;

	unsigned char priority = 0;

	piece const& other_king = p.pieces[1-c][pieces::king];
	int kx = static_cast<int>(new_col) - other_king.column;
	int ky = static_cast<int>(new_row) - other_king.row;
	if( kx <= 1 && kx >= -1 && ky <= 1 && ky >= -1 ) {
		// Other king too close
		return;
	}

	unsigned char target_piece = p.board[new_col][new_row];
	if( target_piece != pieces::nil ) {
		if( (target_piece >> 4) == c ) {
			return;
		}
		priority |= priorities::capture;
	}

	add_if_legal( p, c, moves, check, pieces::king, new_col, new_row, priority );
}

void calc_moves_king( position const& p, color::type c, std::vector<move>& moves, check_info& check )
{
	piece const& pp = p.pieces[c][pieces::king];

	calc_moves_king( p, c, moves, check, 1, 0 );
	calc_moves_king( p, c, moves, check, 1, -1 );
	calc_moves_king( p, c, moves, check, 0, -1 );
	calc_moves_king( p, c, moves, check, -1, -1 );
	calc_moves_king( p, c, moves, check, -1, 0 );
	calc_moves_king( p, c, moves, check, -1, 1 );
	calc_moves_king( p, c, moves, check, 0, 1 );
	calc_moves_king( p, c, moves, check, 1, 1 );

	if( check.check ) {
		return;
	}

	// Queenside castling
	if( p.pieces[c][pieces::rook1].special ) {
		if( p.board[1][pp.row] == pieces::nil && p.board[2][pp.row] == pieces::nil && p.board[3][pp.row] == pieces::nil ) {
			check_info check2;
			detect_check( p, c, check2, 3, pp.row );
			if( !check2.check ) {
				add_if_legal( p, c, moves, check, pieces::king, 2, pp.row, priorities::castle );
			}
		}
	}
	// Kingside castling
	if( p.pieces[c][pieces::rook2].special ) {
		if( p.board[5][pp.row] == pieces::nil && p.board[6][pp.row] == pieces::nil ) {
			check_info check2;
			detect_check( p, c, check2, 5, pp.row );
			if( !check2.check ) {
				add_if_legal( p, c, moves, check, pieces::king, 6, pp.row, priorities::castle );
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, std::vector<move>& moves, check_info& check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; ++cx ) {
		for( int cy = -1; cy <= 1; ++cy ) {
			if( !cx && !cy ) {
				continue;
			}

			int x, y;
			for( x = pp.column + cx, y = pp.row + cy; x >= 0 && x <= 7 && y >= 0 && y <= 7; x += cx, y += cy ) {
				unsigned char target = p.board[x][y];
				if( target == pieces::nil ) {
					add_if_legal( p, c, moves, check, pi, x, y, 0 );
				}
				else {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, moves, check, pi, x, y, priorities::capture );
					}
					break;
				}
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, std::vector<move>& moves, check_info& check )
{
	piece const& pp = p.pieces[c][pieces::queen];
	if( pp.alive ) {
		calc_moves_queen( p, c, moves, check, pieces::queen, pp );
	}
}

void calc_moves_bishop( position const& p, color::type c, std::vector<move>& moves, check_info& check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int cy = -1; cy <= 1; cy += 2 ) {
			int x, y;
			for( x = pp.column + cx, y = pp.row + cy; x >= 0 && x <= 7 && y >= 0 && y <= 7; x += cx, y += cy ) {
				unsigned char target = p.board[x][y];
				if( target == pieces::nil ) {
					add_if_legal( p, c, moves, check, pi, x, y, 0 );
				}
				else {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, moves, check, pi, x, y, priorities::capture );
					}
					break;
				}
			}
		}
	}
}


void calc_moves_bishops( position const& p, color::type c, std::vector<move>& moves, check_info& check )
{
	{
		piece const& pp = p.pieces[c][pieces::bishop1];
		if( pp.alive ) {
			calc_moves_bishop( p, c, moves, check, pieces::bishop1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::bishop2];
		if( pp.alive ) {
			calc_moves_bishop( p, c, moves, check, pieces::bishop2, pp );
		}
	}
}


void calc_moves_rook( position const& p, color::type c, std::vector<move>& moves, check_info& check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int x = pp.column + cx; x >= 0 && x <= 7; x += cx ) {
			unsigned char target = p.board[x][pp.row];
			if( target == pieces::nil ) {
				add_if_legal( p, c, moves, check, pi, x, pp.row, 0 );
			}
			else {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, moves, check, pi, x, pp.row, priorities::capture );
				}
				break;
			}
		}
	}
	for( int cy = -1; cy <= 1; cy += 2 ) {
		for( int y = pp.row + cy; y >= 0 && y <= 7; y += cy ) {
			unsigned char target = p.board[pp.column][y];
			if( target == pieces::nil ) {
				add_if_legal( p, c, moves, check, pi, pp.column, y, 0 );
			}
			else {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, moves, check, pi, pp.column, y, priorities::capture );
				}
				break;
			}
		}
	}
}


void calc_moves_rooks( position const& p, color::type c, std::vector<move>& moves, check_info& check )
{
	{
		piece const& pp = p.pieces[c][pieces::rook1];
		if( pp.alive ) {
			calc_moves_rook( p, c, moves, check, pieces::rook1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::rook2];
		if( pp.alive ) {
			calc_moves_rook( p, c, moves, check, pieces::rook2, pp );
		}
	}
}


void calc_moves_knight( position const& p, color::type c, std::vector<move>& moves, check_info& check, pieces::type pi, piece const& pp, int cx, int cy )
{
	int new_column = cx + pp.column;

	if( new_column < 0 || new_column > 7 ) {
		return;
	}

	int new_row = cy + pp.row;
	if( new_row < 0 || new_row > 7 ) {
		return;
	}

	int priority = 0;

	int new_target = p.board[new_column][new_row];
	if( new_target != pieces::nil ) {
		if( (new_target >> 4) == c ) {
			return;
		}

		priority = priorities::capture;
	}

	add_if_legal( p, c, moves, check, pi, new_column, new_row, priority );
}


void calc_moves_knight( position const& p, color::type c, std::vector<move>& moves, check_info& check, pieces::type pi, piece const& pp )
{
	calc_moves_knight( p, c, moves, check, pi, pp, 2, 1 );
	calc_moves_knight( p, c, moves, check, pi, pp, 2, -1 );
	calc_moves_knight( p, c, moves, check, pi, pp, 1, -2 );
	calc_moves_knight( p, c, moves, check, pi, pp, -1, -2 );
	calc_moves_knight( p, c, moves, check, pi, pp, -2, -1 );
	calc_moves_knight( p, c, moves, check, pi, pp, -2, 1 );
	calc_moves_knight( p, c, moves, check, pi, pp, -1, 2 );
	calc_moves_knight( p, c, moves, check, pi, pp, 1, 2 );
}


void calc_moves_knights( position const& p, color::type c, std::vector<move>& moves, check_info& check )
{
	{
		piece const& pp = p.pieces[c][pieces::knight1];
		if( pp.alive ) {
			calc_moves_knight( p, c, moves, check, pieces::knight1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::knight2];
		if( pp.alive ) {
			calc_moves_knight( p, c, moves, check, pieces::knight2, pp );
		}
	}
}

void calc_diagonal_pawn_move( position const& p, color::type c, std::vector<move>& moves, check_info& check, unsigned int pi, piece const& pp, unsigned char new_col, unsigned char new_row )
{
	unsigned char target = p.board[new_col][new_row];
	if( target == pieces::nil ) {
		if( p.can_en_passant != pieces::nil ) {
			piece const& ep = p.pieces[1-c][p.can_en_passant];
			ASSERT( ep.alive );
			if( ep.column == new_col && ep.row == pp.row ) {

				// Capture en-passant
				add_if_legal( p, c, moves, check, pi, new_col, new_row, priorities::capture );
			}
		}
	}
	else if( (target >> 4) != c ) {
		// Capture!
		add_if_legal( p, c, moves, check, pi, new_col, new_row, priorities::capture );
	}
}

void calc_moves_pawns( position const& p, color::type c, std::vector<move>& moves, check_info& check )
{
	for( unsigned int pi = pieces::pawn1; pi <= pieces::pawn8; ++pi ) {
		piece const& pp = p.pieces[c][pi];
		if( pp.alive ) {
			if( !pp.special ) {
				unsigned char new_row = (c == color::white) ? (pp.row + 1) : (pp.row - 1);
				unsigned char target = p.board[pp.column][new_row];

				if( pp.column > 0 ) {
					unsigned char new_col = pp.column - 1;
					calc_diagonal_pawn_move( p, c, moves, check, pi, pp, new_col, new_row );
				}
				if( pp.column < 7 ) {
					unsigned char new_col = pp.column + 1;
					calc_diagonal_pawn_move( p, c, moves, check, pi, pp, new_col, new_row );
				}

				if( target == pieces::nil ) {

					add_if_legal( p, c, moves, check, pi, pp.column, new_row );

					if( pp.row == ( (c == color::white) ? 1 : 6) ) {
						// Moving two rows from starting row
						new_row = (c == color::white) ? (pp.row + 2) : (pp.row - 2);

						unsigned char target = p.board[pp.column][new_row];
						if( target == pieces::nil ) {
							add_if_legal( p, c, moves, check, pi, pp.column, new_row );
						}
					}
				}
			}
			else {
				// Promoted piece
				unsigned char promoted = (p.promotions[c] >> (2 * (pi - pieces::pawn1) ) ) & 0x03;
				if( promoted == promotions::queen ) {
					calc_moves_queen( p, c, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else if( promoted == promotions::rook ) {
					calc_moves_rook( p, c, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else if( promoted == promotions::bishop ) {
					calc_moves_bishop( p, c, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else {//if( promoted == promotions::queen ) {
					calc_moves_knight( p, c, moves, check, static_cast<pieces::type>(pi), pp );
				}
			}
		}
	}
}

struct {
	bool operator()( move const& lhs, move& rhs ) const {
		return lhs.priority > rhs.priority;
	}
} moveSort;

void calculate_moves( position const& p, color::type c, std::vector<move>& moves, check_info& check )
{
	moves.reserve(30); // Average branching factor. TODO: Analyze performance impact of various reservations
	
	calc_moves_king( p, c, moves, check );

	if( !check.check || !check.multiple ) {
		calc_moves_pawns( p, c, moves, check );
		calc_moves_queen( p, c, moves, check );
		calc_moves_rooks( p, c, moves, check );
		calc_moves_bishops( p, c, moves, check );
		calc_moves_knights( p, c, moves, check );
	}

	std::sort( moves.begin(), moves.end(), moveSort );
}


int step( int depth, position const& p_old, move const& m, color::type c, int alpha, int beta )
{
	position p = p_old;
	bool captured = apply_move( p, m, static_cast<color::type>(1-c) );

	check_info check;
	detect_check( p, c, check );

#if USE_QUIESCENCE == 1
	if( depth <= 0 && !captured && !check.check )
#else
	if( depth <= 0 && !check.check )
#endif
	{
#ifdef USE_STATISTICS
		++stats.evaluated_leaves;
#endif
		return evaluate( c, p, check );
	}

#ifdef USE_STATISTICS
	++stats.evaluated_intermediate;
#endif

	std::vector<move> moves;
	calculate_moves( p, c, moves, check );

	if( moves.empty() ) {
		if( check.check ) {
			return result::loss - depth;
		}
		else {
			return result::draw;
		}
	}

	// Be quiesent, don't decrease search depth after capture.
	// TODO: Maybe add a ply even?
#if USE_QUIESCENCE == 2
	if( !captured )
#endif
	{
		--depth;
	}

	// Exploit fact that std::vector can be treated like an array
	std::size_t count = moves.size();
#ifdef USE_CUTOFF
	if( depth < (CUTOFF_DEPTH) && count > CUTOFF_AMOUNT ) {
		count = CUTOFF_AMOUNT;
	}
#endif
	
	move* mbegin = &moves[0];
	move* mend = mbegin + count;
	for( ; mbegin != mend; ++mbegin ) { // Optimize this, compiler!
		int value = -step( depth, p, *mbegin, static_cast<color::type>(1-c), -beta, -alpha );
		if( value > alpha )
			alpha = value;
		if( alpha >= beta )
			break;
	}

	return alpha;
}

bool calc( position& p, color::type c, move& m )
{
	int depth = MAX_DEPTH;

	int alpha = result::loss - MAX_DEPTH - 1;
	int beta = result::win + MAX_DEPTH + 1;

	check_info check;
	detect_check( p, c, check );

	//std::cout << " " << (check.check ? '1' : '0');
	std::vector<move> moves;
	calculate_moves( p, c, moves, check );

	if( moves.empty() ) {
		if( check.check ) {
			if( c == color::white ) {
				std::cout << "BLACK WINS" << std::endl;
			}
			else {
				std::cout << std::endl << "WHITE WINS" << std::endl;
			}
			return false;
		}
		else {
			if( c == color::black ) {
				std::cout << std::endl;
			}
			std::cout << "DRAW" << std::endl;
			return false;
		}
	}

	// Exploit fact that std::vector can be treated like an array
	std::size_t const count = moves.size();
	
	move* mbegin = &moves[0];
	move* mend = mbegin + count;
	for( ; mbegin != mend; ++mbegin ) { // Optimize this, compiler!
		int value = -step( depth - 1, p, *mbegin, static_cast<color::type>(1-c), -beta, -alpha );
		if( value > alpha ) {
			m = *mbegin;
			alpha = value;
		}
	}

	return true;
}

std::string move_to_string( position const& p, color::type c, move const& m )
{
	std::string ret;

	if( m.piece == pieces::king ) {
		if( m.target_col == 6 && p.pieces[c][m.piece].column == 4 ) {
			return "   0-0";
		}
		else if( m.target_col == 2 && p.pieces[c][m.piece].column == 4 ) {
			return " 0-0-0";
		}
	}

	switch( m.piece ) {
		case pieces::bishop1:
		case pieces::bishop2:
			ret += 'B';
			break;
		case pieces::knight1:
		case pieces::knight2:
			ret += 'N';
			break;
		case pieces::queen:
			ret += 'Q';
			break;
		case pieces::king:
			ret += 'K';
			break;
		case pieces::rook1:
		case pieces::rook2:
			ret += 'R';
			break;
		case pieces::pawn1:
		case pieces::pawn2:
		case pieces::pawn3:
		case pieces::pawn4:
		case pieces::pawn5:
		case pieces::pawn6:
		case pieces::pawn7:
		case pieces::pawn8:
			if( p.pieces[c][m.piece].special ) {
				switch( p.promotions[c] >> (2 * (m.piece - pieces::pawn1) ) & 0x03 ) {
					case promotions::queen:
						ret += 'Q';
						break;
					case promotions::rook:
						ret += 'R';
						break;
					case promotions::bishop:
						ret += 'B';
						break;
					case promotions::knight:
						ret += 'N';
						break;
				}
			}
			else {
				ret += ' ';
			}
		default:
			break;
	}

	ret += 'a' + p.pieces[c][m.piece].column;
	ret += '1' + p.pieces[c][m.piece].row;

	if( p.board[m.target_col][m.target_row] != pieces::nil ) {
		ret += 'x';
	}
	else if( m.piece >= pieces::pawn1 && m.piece <= pieces::pawn8 
		&& p.pieces[c][m.piece].column != m.target_col )
	{
		// Must be en-passant
		ret += 'x';
	}
	else {
		ret += '-';
	}

	ret += 'a' + m.target_col;
	ret += '1' + m.target_row;

	if( m.piece >= pieces::pawn1 && m.piece <= pieces::pawn8 && !p.pieces[c][m.piece].special ) {
		if( m.target_row == 0 || m.target_row == 7 ) {
			ret += "=Q";
		}
		else {
			ret += "  ";
		}
	}
	else {
		ret += "  ";
	}

	return ret;
}

bool validate_move( position const& p, move const& m, color::type c )
{
	check_info check;
	detect_check( p, c, check );

	std::vector<move> moves;
	calculate_moves( p, c, moves, check );

	for( std::vector<move>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
		if( it->piece == m.piece && it->target_col == m.target_col && it->target_row == m.target_row ) {
			return true;
		}
	}

	return false;
}

int main()
{
	std::cout << "  Octochess" << std::endl;
	std::cout << "  ---------" << std::endl;
	std::cout << std::endl;

	srand(123);

	ULONGLONG start = GetTickCount64();
	position p;

	init_board(p);

	int i = 1;
	color::type c = color::white;
	move m = {0};
	while( calc( p, c, m ) ) {
		
		if( c == color::white ) {
			std::cout << std::setw(3) << i << ". ";
		}
		
		std::cout << std::setw(8) << move_to_string(p, c, m);

		if( c == color::black ) {
			++i;
			check_info check = {0};
			int i = evaluate( color::white, p, check );
			//std::cout << "  ; Evaluation: " << i << " centipawns";
			std::cout << std::endl;
		}

		if( !validate_move( p, m, c ) ) {
			std::cerr << std::endl << "NOT A VALID MOVE" << std::endl;
			exit(1);
		}

		apply_move( p, m, c );

		c = static_cast<color::type>(1-c);
	}

	ULONGLONG stop = GetTickCount64();

	std::cerr << std::endl << "Runtime: " << stop - start << " ms " << std::endl;

#ifdef USE_STATISTICS
	std::cerr << "Evaluated positions:    " << stats.evaluated_leaves << std::endl;
	std::cerr << "Intermediate positions: " << stats.evaluated_intermediate << std::endl;
#if USE_QUIESCENCE
	std::cerr << "Quiescent moves:        " << stats.quiescence_moves << std::endl;
#endif
	if( stats.evaluated_intermediate || stats.evaluated_intermediate ) {
		std::cerr << "Time per position:      " << ((stop - start) * 1000 * 1000) / (stats.evaluated_intermediate + stats.evaluated_intermediate) << " ns" << std::endl;
		if( stop != start ) {
			std::cerr << "Positions per second:   " << (1000 * (stats.evaluated_intermediate + stats.evaluated_intermediate)) / (stop - start) << std::endl;
		}
	}
#endif

	return 0;
}
