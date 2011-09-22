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

void do_add_move( position const& p, color::type c, move_info*& moves, pieces2::type const& pi,
				  unsigned char const& old_col, unsigned char const& old_row,
				  unsigned char const& new_col, unsigned char const& new_row,
				  int flags, pieces2::type target, promotions::type promotion = promotions::queen )
{
	move_info mi;

	mi.m.flags = flags;
	mi.m.piece = pi;
	mi.m.source_col = old_col;
	mi.m.source_row = old_row;
	mi.m.target_col = new_col;
	mi.m.target_row = new_row;
	mi.m.captured_piece = target;
	mi.m.promotion = promotion;

	mi.evaluation = get_material_value( target ) * 32 - get_material_value( pi );

	*(moves++) = mi;
}

// Adds the move if it does not result in self getting into check
void add_if_legal( position const& p, color::type c, move_info*& moves, check_map const& check,
				  pieces2::type const& pi,
				  unsigned char const& old_col, unsigned char const& old_row,
				  unsigned char const& new_col, unsigned char const& new_row,
				  int flags, pieces2::type target, promotions::type promotion = promotions::queen )
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

	do_add_move( p, c, moves, pi, old_col, old_row, new_col, new_row, flags, target, promotion );
}

void add_if_legal_king( position const& p, color::type c, move_info*& moves,
					    unsigned char const& old_col, unsigned char const& old_row,
					    unsigned char new_col, unsigned char new_row,
					    int flags, pieces2::type target )
{
	if( detect_check( p, c, new_col, new_row, old_col, old_row ) ) {
		return;
	}

	do_add_move( p, c, moves, pieces2::king, old_col, old_row, new_col, new_row, flags, target );
}

void calc_moves_king( position const& p, color::type c, move_info*& moves, check_map const& check,
					  unsigned char old_col, unsigned char old_row,
					  unsigned char new_col, unsigned char new_row )
{
	if( possible_king_moves[new_row * 8 + new_col] & p.bitboards[1-c].king ) {
		// Other king too close
		return;
	}

	unsigned char target = p.board2[new_col][new_row];
	if( target ) {
		if( (target >> 4) == c ) {
			return;
		}
		add_if_legal_king( p, c, moves, old_col, old_row, new_col, new_row, move_flags::valid, static_cast<pieces2::type>(target & 0x0f) );
	}
}


void calc_moves_king( position const& p, color::type c, move_info*& moves, check_map const& check )
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
		calc_moves_king( p, c, moves, check,
						 old_col, old_row,
						 static_cast<unsigned char>(i % 8), static_cast<unsigned char>(i / 8) );
	}
}


void calc_moves_queen( position const& p, color::type c, move_info*& moves, check_map const& check, unsigned long long queen )
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
				if( target ) {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, moves, check, pieces2::queen, old_col, old_row, x, y, move_flags::valid, static_cast<pieces2::type>(target & 0x0f) );
					}
					break;
				}
			}
		}
	}
}


void calc_moves_queens( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long queens = p.bitboards[c].queens;
	while( queens ) {
		unsigned long long queen;
		bitscan( queens, queen );	
		queens ^= 1ull << queen;
		calc_moves_queen( p, c, moves, check, queen );
	}
}


void calc_moves_bishop( position const& p, color::type c, move_info*& moves, check_map const& check,
					    unsigned long long bishop )
{
	int old_col = static_cast<int>(bishop % 8);
	int old_row = static_cast<int>(bishop / 8);
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int cy = -1; cy <= 1; cy += 2 ) {
			int x, y;
			for( x = old_col + cx, y = old_row + cy; x >= 0 && x <= 7 && y >= 0 && y <= 7; x += cx, y += cy ) {
				unsigned char target = p.board2[x][y];
				if( target ) {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, moves, check, pieces2::bishop, old_col, old_row, x, y, move_flags::valid, static_cast<pieces2::type>(target & 0x0f) );
					}
					break;
				}
			}
		}
	}
}


void calc_moves_bishops( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long bishops = p.bitboards[c].bishops;
	while( bishops ) {
		unsigned long long bishop;
		bitscan( bishops, bishop );	
		bishops ^= 1ull << bishop;
		calc_moves_bishop( p, c, moves, check, bishop );
	}
}


void calc_moves_rook( position const& p, color::type c, move_info*& moves, check_map const& check,
					  unsigned long long rook )
{
	int old_col = static_cast<int>(rook % 8);
	int old_row = static_cast<int>(rook / 8);

	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int x = old_col + cx; x >= 0 && x <= 7; x += cx ) {
			unsigned char target = p.board2[x][old_row];
			if( target ) {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, moves, check, pieces2::rook, old_col, old_row, x, old_row, move_flags::valid, static_cast<pieces2::type>(target & 0x0f) );
				}
				break;
			}
		}
	}
	for( int cy = -1; cy <= 1; cy += 2 ) {
		for( int y = old_row + cy; y >= 0 && y <= 7; y += cy ) {
			unsigned char target = p.board2[old_col][y];
			if( target ) {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, moves, check, pieces2::rook, old_col, old_row, old_col, y, move_flags::valid, static_cast<pieces2::type>(target & 0x0f) );
				}
				break;
			}
		}
	}
}


void calc_moves_rooks( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long rooks = p.bitboards[c].rooks;
	while( rooks ) {
		unsigned long long rook;
		bitscan( rooks, rook );
		rooks ^= 1ull << rook;
		calc_moves_rook( p, c, moves, check, rook );
	}
}


void calc_moves_knight( position const& p, color::type c, move_info*& moves, check_map const& check,
					    unsigned char old_col, unsigned char old_row,
						unsigned char new_col, unsigned char new_row )
{
	int target = p.board2[new_col][new_row];
	if( target ) {
		if( (target >> 4) == c ) {
			return;
		}
		add_if_legal( p, c, moves, check, pieces2::knight, old_col, old_row, new_col, new_row, move_flags::valid, static_cast<pieces2::type>(target & 0x0f) );
	}

}

void calc_moves_knight( position const& p, color::type c, move_info*& moves, check_map const& check,
					    unsigned long long old_knight )
{
	unsigned char old_col = static_cast<unsigned char>(old_knight % 8);
	unsigned char old_row = static_cast<unsigned char>(old_knight / 8);

	unsigned long long new_knights = possible_knight_moves[old_knight];
	while( new_knights ) {
		unsigned long long new_knight;
		bitscan( new_knights, new_knight );
		new_knights ^= 1ull << new_knight;
		calc_moves_knight( p, c, moves, check,
						   old_col, old_row,
						   static_cast<unsigned char>(new_knight % 8), static_cast<unsigned char>(new_knight / 8) );
	}
}


void calc_moves_knights( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long knights = p.bitboards[c].knights;
	while( knights ) {
		unsigned long long knight;
		bitscan( knights, knight );
		knights ^= 1ull << knight;
		calc_moves_knight( p, c, moves, check, knight );
	}
}

void calc_diagonal_pawn_move( position const& p, color::type c, move_info*& moves, check_map const& check,
							  unsigned char old_col, unsigned char old_row,
							  unsigned char new_col, unsigned char new_row )
{
	unsigned char target = p.board2[new_col][new_row];
	if( !target ) {
		if( p.can_en_passant && (p.can_en_passant >> 6) != c ) {
			unsigned char ep_col = p.can_en_passant % 8;
			unsigned char ep_row = (p.can_en_passant / 8) & 7;
			if( ep_col == new_col && ep_row == old_row ) {
				// Capture en-passant

				// Special case: Cannot use normal check from add_if_legal as target square is not piece square and if captured pawn gives check, bad things happen.
				unsigned char const& cv_old = check.board[old_col][old_row];
				unsigned char const& cv_new = check.board[new_col][new_row];
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
				unsigned long long kings = p.bitboards[c].king;
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
						unsigned char t = p.board2[col][old_row];
						if( !t ) {
							// Empty square
							continue;
						}
						if( ( t >> 4) == c ) {
							// Own piece
							break;
						}
						t &= 0x0f;
						if( t == pieces2::queen || t == pieces2::rook ) {
							// Not a legal move unfortunately
							return;
						}

						// Harmless piece
						break;
					}
				}

				do_add_move( p, c, moves, pieces2::pawn, old_col, old_row, new_col, new_row, move_flags::valid | move_flags::enpassant, pieces2::pawn );
			}
		}
	}
	else if( (target >> 4) != c ) {
		// Capture!
		int flags = move_flags::valid;
		if( new_row == 0 || new_row == 7 ) {
			flags |= move_flags::promotion;
		}
		add_if_legal( p, c, moves, check, pieces2::pawn, old_col, old_row, new_col, new_row, flags, static_cast<pieces2::type>(target & 0x0f) );
	}
}

void calc_moves_pawn( position const& p, color::type c, move_info*& moves, check_map const& check,
					  unsigned long long pawn )
{
	unsigned char old_col = static_cast<unsigned char>(pawn % 8);
	unsigned char old_row = static_cast<unsigned char>(pawn / 8);
	unsigned char new_row = (c == color::white) ? (old_row + 1) : (old_row - 1);
	
	if( old_col > 0 ) {
		unsigned char new_col = old_col - 1;
		calc_diagonal_pawn_move( p, c, moves, check, old_col, old_row, new_col, new_row );
	}
	if( old_col < 7 ) {
		unsigned char new_col = old_col + 1;
		calc_diagonal_pawn_move( p, c, moves, check, old_col, old_row, new_col, new_row );
	}
}

void calc_moves_pawns( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long pawns = p.bitboards[c].pawns;
	while( pawns ) {
		unsigned long long pawn;
		bitscan( pawns, pawn );
		pawns ^= 1ull << pawn;

		calc_moves_pawn( p, c, moves, check, pawn );
	}
}
}

void calculate_moves_captures( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	calc_moves_king( p, c, moves, check );

	calc_moves_pawns( p, c, moves, check );
	calc_moves_queens( p, c, moves, check );
	calc_moves_rooks( p, c, moves, check );
	calc_moves_bishops( p, c, moves, check );
	calc_moves_knights( p, c, moves, check );
}
