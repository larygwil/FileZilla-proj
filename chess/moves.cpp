#include "assert.hpp"
#include "moves.hpp"
#include "eval.hpp"
#include "util.hpp"

#include <algorithm>
#include <iostream>
#include <string>

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

	mi.m.piece = pi;
	mi.m.target_col = new_col;
	mi.m.target_row = new_row;

	mi.evaluation = evaluate_move( p, c, current_evaluation, mi.m );
	mi.random = get_random_unsigned_char();

	*(moves++) = mi;
}

void add_if_legal_king( position const& p, color::type c, int const current_evaluation,  move_info*& moves, unsigned char new_col, unsigned char new_row )
{
	piece const& kp = p.pieces[c][pieces::king];
	if( detect_check( p, c, new_col, new_row, kp.column, kp.row ) ) {
		return;
	}

	move_info mi;
	mi.m.piece = pieces::king;
	mi.m.target_col = new_col;
	mi.m.target_row = new_row;

	mi.evaluation = evaluate_move( p, c, current_evaluation, mi.m );
	mi.random = get_random_unsigned_char();

	*(moves++) = mi;
}


void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, signed char cx, signed char cy )
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
	}

	add_if_legal_king( p, c, current_evaluation, moves, new_col, new_row );
}

void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	piece const& pp = p.pieces[c][pieces::king];

	calc_moves_king( p, c, current_evaluation, moves, check, 1, 0 );
	calc_moves_king( p, c, current_evaluation, moves, check, 1, -1 );
	calc_moves_king( p, c, current_evaluation, moves, check, 0, -1 );
	calc_moves_king( p, c, current_evaluation, moves, check, -1, -1 );
	calc_moves_king( p, c, current_evaluation, moves, check, -1, 0 );
	calc_moves_king( p, c, current_evaluation, moves, check, -1, 1 );
	calc_moves_king( p, c, current_evaluation, moves, check, 0, 1 );
	calc_moves_king( p, c, current_evaluation, moves, check, 1, 1 );

	if( check.check ) {
		return;
	}

// Queenside castling
	if( p.pieces[c][pieces::rook1].special ) {
		if( p.board[1][pp.row] == pieces::nil && p.board[2][pp.row] == pieces::nil && p.board[3][pp.row] == pieces::nil ) {
			if( !detect_check( p, c, 3, pp.row, 3, pp.row ) ) {
				add_if_legal_king( p, c, current_evaluation, moves, 2, pp.row );
			}
		}
	}
	// Kingside castling
	if( p.pieces[c][pieces::rook2].special ) {
		if( p.board[5][pp.row] == pieces::nil && p.board[6][pp.row] == pieces::nil ) {
			if( !detect_check( p, c, 5, pp.row, 5, pp.row ) ) {
				add_if_legal_king( p, c, current_evaluation, moves, 6, pp.row );
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
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
					add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
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


void calc_moves_queen( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	piece const& pp = p.pieces[c][pieces::queen];
	if( pp.alive ) {
		calc_moves_queen( p, c, current_evaluation, moves, check, pieces::queen, pp );
	}
}

void calc_moves_bishop( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int cy = -1; cy <= 1; cy += 2 ) {
			int x, y;
			for( x = pp.column + cx, y = pp.row + cy; x >= 0 && x <= 7 && y >= 0 && y <= 7; x += cx, y += cy ) {
				unsigned char target = p.board[x][y];
				if( target == pieces::nil ) {
					add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
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


void calc_moves_bishops( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	{
		piece const& pp = p.pieces[c][pieces::bishop1];
		if( pp.alive ) {
			calc_moves_bishop( p, c, current_evaluation, moves, check, pieces::bishop1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::bishop2];
		if( pp.alive ) {
			calc_moves_bishop( p, c, current_evaluation, moves, check, pieces::bishop2, pp );
		}
	}
}


void calc_moves_rook( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int x = pp.column + cx; x >= 0 && x <= 7; x += cx ) {
			unsigned char target = p.board[x][pp.row];
			if( target == pieces::nil ) {
				add_if_legal( p, c, current_evaluation, moves, check, pi, x, pp.row );
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
				add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, y );
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


void calc_moves_rooks( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	{
		piece const& pp = p.pieces[c][pieces::rook1];
		if( pp.alive ) {
			calc_moves_rook( p, c, current_evaluation, moves, check, pieces::rook1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::rook2];
		if( pp.alive ) {
			calc_moves_rook( p, c, current_evaluation, moves, check, pieces::rook2, pp );
		}
	}
}


void calc_moves_knight( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp, int cx, int cy )
{
	int new_column = cx + pp.column;

	if( new_column < 0 || new_column > 7 ) {
		return;
	}

	int new_row = cy + pp.row;
	if( new_row < 0 || new_row > 7 ) {
		return;
	}

	int new_target = p.board[new_column][new_row];
	if( new_target != pieces::nil ) {
		if( (new_target >> 4) == c ) {
			return;
		}
	}

	add_if_legal( p, c, current_evaluation, moves, check, pi, new_column, new_row );
}


void calc_moves_knight( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
{
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, 2, 1 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, 2, -1 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, 1, -2 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, -1, -2 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, -2, -1 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, -2, 1 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, -1, 2 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, 1, 2 );
}


void calc_moves_knights( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	{
		piece const& pp = p.pieces[c][pieces::knight1];
		if( pp.alive ) {
			calc_moves_knight( p, c, current_evaluation, moves, check, pieces::knight1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::knight2];
		if( pp.alive ) {
			calc_moves_knight( p, c, current_evaluation, moves, check, pieces::knight2, pp );
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
							std::cerr << "ENPASSANT SPECIAL" << std::endl;
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
								std::cerr << "ENPASSANT SPECIAL" << std::endl;
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

void calc_moves_pawns( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	for( unsigned int pi = pieces::pawn1; pi <= pieces::pawn8; ++pi ) {
		piece const& pp = p.pieces[c][pi];
		if( pp.alive ) {
			if( !pp.special ) {
				unsigned char new_row = (c == color::white) ? (pp.row + 1) : (pp.row - 1);
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

					add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, new_row );

					if( pp.row == ( (c == color::white) ? 1 : 6) ) {
						// Moving two rows from starting row
						new_row = (c == color::white) ? (pp.row + 2) : (pp.row - 2);

						unsigned char target = p.board[pp.column][new_row];
						if( target == pieces::nil ) {
							add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, new_row );
						}
					}
				}
			}
			else {
				// Promoted piece
				unsigned char promoted = (p.promotions[c] >> (2 * (pi - pieces::pawn1) ) ) & 0x03;
				if( promoted == promotions::queen ) {
					calc_moves_queen( p, c, current_evaluation, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else if( promoted == promotions::rook ) {
					calc_moves_rook( p, c, current_evaluation, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else if( promoted == promotions::bishop ) {
					calc_moves_bishop( p, c, current_evaluation, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else {//if( promoted == promotions::knight ) {
					calc_moves_knight( p, c, current_evaluation, moves, check, static_cast<pieces::type>(pi), pp );
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

void calculate_moves( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	move_info* start = moves;

	calc_moves_king( p, c, current_evaluation, moves, check );

	if( !check.check || !check.multiple() )
	{
		calc_moves_pawns( p, c, current_evaluation, moves, check );
		calc_moves_queen( p, c, current_evaluation, moves, check );
		calc_moves_rooks( p, c, current_evaluation, moves, check );
		calc_moves_bishops( p, c, current_evaluation, moves, check );
		calc_moves_knights( p, c, current_evaluation, moves, check );
	}

	std::sort( start, moves, moveSort );
}
