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
#include "detect_check.hpp"

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <assert.h>

#if _MSC_VER
  #include "windows.hpp"
#else
  #include "unix.hpp"
#endif

#include <unordered_map>
#include <map>

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
	std::cerr << "Assertion failed (" << __FILE__ << ":" << __LINE__ << "): " << #x << std::endl; \
	for(;;){}\
		abort(); \
	} \
	break; \
} while( true );
#undef ASSERT
#ifndef ASSERT
#define ASSERT(x)
#endif

struct hash_getter
{
unsigned long long operator()( position_base const& p ) const
{
	unsigned long long hash = 0;

	char const* c = reinterpret_cast<char const*>(&p);
	char const* end = c + sizeof( position_base );
	for( ; c != end; ++c ) {
		hash += *c;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

//	hash = *reinterpret_cast<const unsigned long long*>(reinterpret_cast<const char*>(&p.pieces[0][0]));
//	hash ^= *reinterpret_cast<const unsigned long long*>(reinterpret_cast<const char*>(&p.pieces[0][0]) + 8);
//	hash ^= *reinterpret_cast<const unsigned long long*>(reinterpret_cast<const char*>(&p.pieces[0][0]) + 16);
//	hash ^= *reinterpret_cast<const unsigned long long*>(reinterpret_cast<const char*>(&p.pieces[0][0]) + 24);
//	hash ^= p.promotions[0];
//	hash ^= p.promotions[1];

	return hash;
}
};

struct step_data {
	int evaluation;
	bool terminal;
	check_info check;
	move best_move;
	int remaining_depth;
	int alpha;
	int beta;
};

typedef std::unordered_map<position_base, step_data, hash_getter> data_maps;
data_maps data_map[2];

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

int evaluate( color::type c, position const& p, check_info const& check, int depth )
{
	int value = evaluate_side( c, p, check ) - evaluate_side( static_cast<color::type>(1-c), p, check );

	value *= 100;

//	if( value > 0 ) {
//		value += depth;
//	}
//	else {
//		value -= depth;
//	}

	ASSERT( value > result::loss && value < result::win );
	return value;
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
				p.board[6][m.target_row] = pieces::king | (c << 4);
				p.board[7][m.target_row] = pieces::nil;
				p.board[5][m.target_row] = pieces::rook2 | (c << 4);
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
				p.board[2][m.target_row] = pieces::king | (c << 4);
				p.board[0][m.target_row] = pieces::nil;
				p.board[3][m.target_row] = pieces::rook1 | (c << 4);
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
void add_if_legal( position const& p, color::type c, std::vector<move>& moves, unsigned char pi, unsigned char new_col, unsigned char new_row, unsigned char priority = 0 )
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

	m.priority = priority | ((rand() % 0x0e)+1);

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

	add_if_legal( p, c, moves, pieces::king, new_col, new_row, priority );
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
				add_if_legal( p, c, moves, pieces::king, 2, pp.row, priorities::castle );
			}
		}
	}
	// Kingside castling
	if( p.pieces[c][pieces::rook2].special ) {
		if( p.board[5][pp.row] == pieces::nil && p.board[6][pp.row] == pieces::nil ) {
			check_info check2;
			detect_check( p, c, check2, 5, pp.row );
			if( !check2.check ) {
				add_if_legal( p, c, moves, pieces::king, 6, pp.row, priorities::castle );
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
					add_if_legal( p, c, moves, pi, x, y, 0 );
				}
				else {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, moves, pi, x, y, priorities::capture );
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
					add_if_legal( p, c, moves, pi, x, y, 0 );
				}
				else {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, moves, pi, x, y, priorities::capture );
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
				add_if_legal( p, c, moves, pi, x, pp.row, 0 );
			}
			else {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, moves, pi, x, pp.row, priorities::capture );
				}
				break;
			}
		}
	}
	for( int cy = -1; cy <= 1; cy += 2 ) {
		for( int y = pp.row + cy; y >= 0 && y <= 7; y += cy ) {
			unsigned char target = p.board[pp.column][y];
			if( target == pieces::nil ) {
				add_if_legal( p, c, moves, pi, pp.column, y, 0 );
			}
			else {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, moves, pi, pp.column, y, priorities::capture );
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

	add_if_legal( p, c, moves, pi, new_column, new_row, priority );
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
				add_if_legal( p, c, moves, pi, new_col, new_row, priorities::capture );
			}
		}
	}
	else if( (target >> 4) != c ) {
		// Capture!
		add_if_legal( p, c, moves, pi, new_col, new_row, priorities::capture );
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

					add_if_legal( p, c, moves, pi, pp.column, new_row );

					if( pp.row == ( (c == color::white) ? 1 : 6) ) {
						// Moving two rows from starting row
						new_row = (c == color::white) ? (pp.row + 2) : (pp.row - 2);

						unsigned char target = p.board[pp.column][new_row];
						if( target == pieces::nil ) {
							add_if_legal( p, c, moves, pi, pp.column, new_row );
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

struct MoveSort {
	bool operator()( move const& lhs, move const& rhs ) const {
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


std::string move_to_string( position const& p, color::type c, move const& m )
{
	std::string ret;

	if( m.piece == pieces::king ) {
		if( m.target_col == 6 && p.pieces[c][m.piece].column == 4 ) {
			return "   O-O  ";
		}
		else if( m.target_col == 2 && p.pieces[c][m.piece].column == 4 ) {
			return " O-O-O  ";
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
			break;
		default:
			break;
	}

	ret += 'a' + p.pieces[c][m.piece].column;
	ret += '1' + p.pieces[c][m.piece].row;

	if( p.board[m.target_col][m.target_row] != pieces::nil ) {
		ret += 'x';
	}
	else if( m.piece >= pieces::pawn1 && m.piece <= pieces::pawn8
		&& p.pieces[c][m.piece].column != m.target_col && !p.pieces[c][m.piece].special )
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


int step( int depth, int const max_depth, position p, move const& m, color::type c, int alpha, int beta )
{
	bool captured = apply_move( p, m, static_cast<color::type>(1-c) );

#if USE_QUIESCENCE
	int limit;
	if( captured ) {
		limit = max_depth + USE_QUIESCENCE;
	}
	else {
		limit = max_depth;
	}
#else
	int const limit = max_depth;
#endif

#if USE_TRANSPOSITION
	move old_best;
	data_maps::const_iterator it = data_map[c].end();//find( p );
	check_info check;
	if( it != data_map[c].end() ) {
		if( it->second.terminal ) {
			if( it->second.evaluation == result::draw ) {
				return result::draw;
			}
			else {
				return result::loss + depth;
			}
		}
//		else if( it->second.evaluation > result::win_threshold && alpha <= it->second.alpha && beta >= it->second.beta ) {
//			return std::max( alpha, it->second.evaluation );
//		}
//		else if( (limit - depth) <= (result::loss - it->second.evaluation) && it->second.evaluation < result::loss_threshold ) {//&& alpha <= it->second.alpha && beta >= it->second.beta ) {
//			return std::max( alpha, it->second.evaluation );
//		}
//		else if( (limit - depth) <= it->second.remaining_depth && alpha <= it->second.alpha && beta >= it->second.beta ) {
//			return std::max( alpha, it->second.evaluation );
//		}
		old_best = it->second.best_move;
		check = it->second.check;
	}
	else {
		old_best.priority = 0;
		detect_check( p, c, check );
	}
#else
	check_info check;
	detect_check( p, c, check );
#endif

#if USE_QUIESCENCE
	if( depth >= limit && !check.check )
#else
	if( depth >= max_depth && !check.check )
#endif
	{
#ifdef USE_STATISTICS
		++stats.evaluated_leaves;
#endif
		return evaluate( c, p, check, depth );
	}

#ifdef USE_STATISTICS
	++stats.evaluated_intermediate;
#endif

	std::vector<move> moves;
	calculate_moves( p, c, moves, check );

#if USE_TRANSPOSITION
	step_data d = {0};
	d.check = check;
	d.beta = beta;
	d.alpha = alpha;
	d.remaining_depth = limit - depth;
#endif

	if( moves.empty() ) {
#if USE_TRANSPOSITION
		ASSERT( !old_best.priority );
#endif
		if( check.check ) {
#if USE_TRANSPOSITION
			d.terminal = true;
			d.evaluation = result::loss + limit - depth + 1;
			data_map[c][p] = d;
#endif
			return result::loss + depth;
		}
		else {
#if USE_TRANSPOSITION
			d.terminal = true;
			d.evaluation = result::draw;
			data_map[c][p] = d;
#endif
			return result::draw;
		}
	}
#if USE_TRANSPOSITION
	else {
		d.terminal = false;
	}
#endif

	// Be quiesent, don't decrease search depth after capture.
	// TODO: Maybe add a ply even?
#if USE_QUIESCENCE == 2
	if( !captured )
#endif
	{
		++depth;
	}

	// Exploit fact that std::vector can be treated like an array
	std::size_t count = moves.size();
#ifdef USE_CUTOFF
	if( depth < (CUTOFF_DEPTH) && count > CUTOFF_AMOUNT ) {
		count = CUTOFF_AMOUNT;
	}
#endif

	int best_value = result::loss;

#if USE_TRANSPOSITION
	if( old_best.priority ) {
		int value = -step( depth, max_depth, p, old_best, static_cast<color::type>(1-c), -beta, -alpha );
		if( value > best_value ) {
			best_value = value;
			d.best_move = old_best;

			if( value > alpha ) {
				alpha = value;
			}
		}
	}
#endif

	move* mbegin = &moves[0];
	move* mend = mbegin + count;
	for( ; mbegin != mend; ++mbegin ) { // Optimize this, compiler!
		int value = -step( depth, max_depth, p, *mbegin, static_cast<color::type>(1-c), -beta, -alpha );
		if( value > best_value ) {
			best_value = value;

#if USE_TRANSPOSITION
			d.best_move = *mbegin;
#endif

			if( value > alpha ) {
				alpha = value;
			}
		}
		if( alpha >= beta )
			break;
	}

#if USE_TRANSPOSITION
	d.evaluation = best_value;
	data_map[c][p] = d;
#endif
	return alpha;
}



bool calc( position& p, color::type c, move& m, int& res )
{
	check_info check;
	detect_check( p, c, check );

	//std::cout << " " << (check.check ? '1' : '0');
	std::vector<move> moves;
	calculate_moves( p, c, moves, check );

	if( moves.empty() ) {
		if( check.check ) {
			if( c == color::white ) {
				std::cerr << "BLACK WINS" << std::endl;
				res = result::loss;
			}
			else {
				std::cerr << std::endl << "WHITE WINS" << std::endl;
				res = result::win;
			}
			return false;
		}
		else {
			if( c == color::black ) {
				std::cout << std::endl;
			}
			std::cerr << "DRAW" << std::endl;
			res = result::draw;
			return false;
		}
	}

	// Exploit fact that std::vector can be treated like an array
	std::size_t const count = moves.size();

	{
		std::cerr << "Possible moves:";
		move* mbegin = &moves[0];
		move* mend = mbegin + count;
		for( ; mbegin != mend; ++mbegin ) { // Optimize this, compiler!
			std::cerr << " " << move_to_string( p, c, *mbegin );
		}
		std::cerr << std::endl;
	}

#if USE_TRANSPOSITION
	data_map[0].clear();
	data_map[1].clear();

	for( int i = 5; i <= MAX_DEPTH; ++i )
	{
		int max_depth = i;
#else
	{
		int max_depth = MAX_DEPTH;
#endif

		int alpha = result::loss;
		int beta = result::win;

		move* mbegin = &moves[0];
		move* mend = mbegin + count;
		for( ; mbegin != mend; ++mbegin ) { // Optimize this, compiler!
			int value = -step( 1, max_depth, p, *mbegin, static_cast<color::type>(1-c), -beta, -alpha );
			if( max_depth == MAX_DEPTH ) {
				std::cerr << value << " " << move_to_string( p, c, *mbegin ) << std::endl;
			}

			if( value > alpha ) {
				m = *mbegin;
				alpha = value;
			}
		}
		if( alpha < result::loss_threshold || alpha > result::win_threshold ) {
			std::cerr << "Early break at " << i << std::endl;
			break;
		}
	}

	return true;
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


void auto_play() {
	unsigned long long start = get_time();
	position p;

	init_board(p);

	int i = 1;
	color::type c = color::white;
	move m = {0};
	int res = 0;
	while( calc( p, c, m, res ) ) {
		if( c == color::white ) {
			std::cout << std::setw(3) << i << ".";
		}
		
		std::cout << " " << move_to_string(p, c, m);

		if( c == color::black ) {
			++i;
			check_info check = {0};
			int i = evaluate( color::white, p, check, 0 );
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

	unsigned long long stop = get_time();

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
}


void parse_move( position& p, color::type& c, std::string const& line ) {
	if( line.size() < 4 || line.size() > 5 ) {
		std::cout << "Error (unknown command): " << line << std::endl;
		return;
	}

	if( line[0] < 'a' || line[0] > 'h') {
		std::cout << "Error (unknown command): " << line << std::endl;
		return;
	}
	if( line[1] < '1' || line[1] > '8') {
		std::cout << "Error (unknown command): " << line << std::endl;
		return;
	}
	if( line[2] < 'a' || line[2] > 'h') {
		std::cout << "Error (unknown command): " << line << std::endl;
		return;
	}
	if( line[3] < '1' || line[3] > '8') {
		std::cout << "Error (unknown command): " << line << std::endl;
		return;
	}

	if( line.size() == 5 && line[4] != 'q' ) {
		std::cout << "Illegal move (not promoting to queen is disallowed due to lazy developer): " << line << std::endl;
		return;
	}

	int col = line[0] - 'a';
	int row = line[1] - '1';

	unsigned char pi = p.board[col][row];
	if( pi == pieces::nil ) {
		std::cout << "Illegal move (no piece in source square): " << line << std::endl;
		return;
	}
	if( (pi >> 4) != c ) {
		std::cout << "Illegal move (piece in source square of wrong color): " << line << std::endl;
		return;
	}
	pi &= 0xf;

	move m;
	m.piece = pi;
	m.priority = 0;
	m.target_col = line[2] - 'a';
	m.target_row = line[3] - '1';

	if( !validate_move( p, m, c ) ) {
		std::cout << "Illegal move: " << line << std::endl;
		return;
	}

	apply_move( p, m, c );
	c = static_cast<color::type>( 1 - c );
}

void xboard()
{
	position p;

	init_board(p);

	std::string line;
	std::getline( std::cin, line );
	if( line != "xboard" ) {
		std::cerr << "First command needs to be xboard!" << std::endl;
		exit(1);
	}
	std::cout << std::endl;

	color::type c = color::white;

	while( true ) {
		std::getline( std::cin, line );
		if( line == "force" ) {
			// Ignore
		}
		else if( line == "go" ) {
			// Do a step
			move m;
			int res = 0;
			if( calc( p, c, m, res ) ) {

				std::cout << "move " << move_to_string( p, c, m ) << std::endl;

				apply_move( p, m, c );

				c = static_cast<color::type>( 1 - c );
			}
			else {
				if( res == result::win ) {
					std::cout << "1-0 (White wins)" << std::endl;
				}
				else if( res == result::loss ) {
					std::cout << "0-1 (Black wins)" << std::endl;
				}
				else {
					std::cout << "1/2-1/2 (Draw)" << std::endl;
				}
			}
		}
		else {
			parse_move( p, c, line );
		}
	}
}

int main( int argc, char *argv[] )
{
	console_init();

	std::cerr << "  Octochess" << std::endl;
	std::cerr << "  ---------" << std::endl;
	std::cerr << std::endl;

	srand(1234);

	if( argc >= 2 && !strcmp(argv[1], "--xboard" ) ) {
		xboard();
	}
	else {
		auto_play();
	}
}

