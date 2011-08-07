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

	mi.evaluation = evaluate_move( p, c, current_evaluation, mi.m );
	mi.random = get_random_unsigned_char();

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

	mi.evaluation = evaluate_move( p, c, current_evaluation, mi.m );
	mi.random = get_random_unsigned_char();

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
		if( (target_piece >> 4) == c ) {
			return;
		}
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
	int i;
	while( (i = __builtin_ffsll( kings ) ) ) {
		--i;
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
				if( target == pieces::nil ) {
					if( inverse_check.board[pp.column][pp.row] || inverse_check.board[x][y] ) {
						add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
					}
				}
				else {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
					}
					break;
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
				if( target == pieces::nil ) {
					if( inverse_check.board[pp.column][pp.row] ||
						inverse_check.board[x][y] == 0xc0 )
					{
						add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
					}
				}
				else {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
					}
					break;
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
			if( target == pieces::nil ) {
				if( inverse_check.board[pp.column][pp.row] || inverse_check.board[x][pp.row] == 0x80 ) {
					add_if_legal( p, c, current_evaluation, moves, check, pi, x, pp.row );
				}
			}
			else {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, current_evaluation, moves, check, pi, x, pp.row );
				}
				break;
			}
		}
	}
	for( int cy = -1; cy <= 1; cy += 2 ) {
		for( int y = pp.row + cy; y >= 0 && y <= 7; y += cy ) {
			unsigned char target = p.board[pp.column][y];
			if( target == pieces::nil ) {
				if( inverse_check.board[pp.column][pp.row] || inverse_check.board[pp.column][y] == 0x80 ) {
					add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, y );
				}
			}
			else {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, y );
				}
				break;
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
		if( (new_target >> 4) == c ) {
			return;
		}
	}
	else {
		if( !inverse_check.board[pp.column][pp.row] ) {
			unsigned long long mask = 1ull << (p.pieces[1-c][pieces::king].column + p.pieces[1-c][pieces::king].row * 8);
			if( !(possible_knight_moves[new_column + new_row * 8] & mask ) ) {
				return;
			}
		}
	}

	add_if_legal( p, c, current_evaluation, moves, check, pi, new_column, new_row );
}


void calc_moves_knight( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check, pieces::type pi, piece const& pp )
{
	unsigned long long knights = possible_knight_moves[pp.column + pp.row * 8];
	int i;
	while( (i = __builtin_ffsll( knights ) ) ) {
		--i;
		knights ^= 1ull << i;
		calc_moves_knight( p, c, current_evaluation, moves, check, inverse_check, pi, pp, i & 0x7, i >> 3 );
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

void calc_diagonal_pawn_move( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, unsigned int pi, piece const& pp, unsigned char new_col, unsigned char new_row )
{
	unsigned char target = p.board[new_col][new_row];
	if( target == pieces::nil ) {
		if( p.can_en_passant != pieces::nil ) {
			piece const& ep = p.pieces[1-c][p.can_en_passant];
			ASSERT( ep.alive );
			if( ep.column == new_col && ep.row == pp.row ) {

				// Capture en-passant

				// Special case: black queen, black pawn, white pawn, white king from left to right on rank 5. Capturing opens up check!
				piece const& kp = p.pieces[c][pieces::king];
				if( kp.row == pp.row ) {
					signed char cx = static_cast<signed char>(pp.column) - kp.column;
					if( cx > 0 ) {
						cx = 1;
					}
					else {
						cx = -1;
					}
					for( signed char col = pp.column + cx; col < 8 && col>= 0; col += cx ) {
						if( col == new_col ) {
							continue;
						}
						unsigned char t = p.board[col][pp.row];
						if( t == pieces::nil ) {
							continue;
						}
						if( ( t >> 4) == c ) {
							// Own piece
							break;
						}
						t &= 0x0f;
						if( t == pieces::queen || t == pieces::rook1 || t == pieces::rook2 ) {
							// Not a legal move unfortunately
							return;
						}
						else if( t >= pieces::pawn1 && t <= pieces::pawn8 ) {
							piece const& pawn = p.pieces[1-c][t];
							if( !pawn.special ) {
								// Harmless pawn
							}

							unsigned short promoted = (p.promotions[1-c] >> (2 * (t - pieces::pawn1) ) ) & 0x03;
							if( promoted == promotions::queen || promoted == promotions::rook ) {
								// Not a legal move unfortunately
								return;
							}
						}

						// Harmless piece
						break;
					}
				}
				add_if_legal( p, c, current_evaluation, moves, check, pi, new_col, new_row );
			}
		}
	}
	else if( (target >> 4) != c ) {
		// Capture!
		add_if_legal( p, c, current_evaluation, moves, check, pi, new_col, new_row );
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

				if( pp.column > 0 ) {
					unsigned char new_col = pp.column - 1;
					calc_diagonal_pawn_move( p, c, current_evaluation, moves, check, pi, pp, new_col, new_row );
				}
				if( pp.column < 7 ) {
					unsigned char new_col = pp.column + 1;
					calc_diagonal_pawn_move( p, c, current_evaluation, moves, check, pi, pp, new_col, new_row );
				}

				if( target == pieces::nil ) {

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

struct MoveSort {
	bool operator()( move_info const& lhs, move_info const& rhs ) const {
		if( lhs.evaluation > rhs.evaluation ) {
			return true;
		}
		if( lhs.evaluation < rhs.evaluation ) {
			return false;
		}

		return lhs.random > rhs.random;
	}
} moveSort;

}

void calculate_moves_captures_and_checks( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check )
{
	move_info* start = moves;

	calc_moves_king( p, c, current_evaluation, moves, check, inverse_check );

	calc_moves_pawns( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_queen( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_rooks( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_bishops( p, c, current_evaluation, moves, check, inverse_check );
	calc_moves_knights( p, c, current_evaluation, moves, check, inverse_check );

	std::sort( start, moves, moveSort );
}
