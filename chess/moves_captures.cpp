#include "assert.hpp"
#include "moves.hpp"
#include "eval.hpp"
#include "util.hpp"
#include "calc.hpp"
#include "sliding_piece_attacks.hpp"

#include <algorithm>
#include <iostream>
#include <string>

extern unsigned long long const pawn_control[2][64];

extern unsigned long long const possible_king_moves[];
extern unsigned long long const possible_knight_moves[];

extern unsigned long long pawn_enpassant[2];

namespace {

void do_add_move( position const& p, color::type c, move_info*& moves, pieces::type const& pi,
				  unsigned char const& source, unsigned char const& target,
				  int flags, pieces::type captured, promotions::type promotion = promotions::queen )
{
	move_info& mi = *(moves++);

	mi.m.flags = flags;
	mi.m.piece = pi;
	mi.m.source = source;
	mi.m.target = target;
	mi.m.captured_piece = captured;
	mi.m.promotion = promotion;

	mi.evaluation = get_material_value( captured ) * 32 - get_material_value( pi );
}

// Adds the move if it does not result in self getting into check
void add_if_legal( position const& p, color::type c, move_info*& moves, check_map const& check,
				  pieces::type const& pi,
				  unsigned char const& source, unsigned char const& target,
				  int flags, pieces::type captured, promotions::type promotion = promotions::queen )
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

	do_add_move( p, c, moves, pi, source, target, flags, captured, promotion );
}

void add_if_legal_king( position const& p, color::type c, move_info*& moves,
						unsigned char const& source, unsigned char const& target,
						int flags, pieces::type captured )
{
	if( detect_check( p, c, target, source ) ) {
		return;
	}

	do_add_move( p, c, moves, pieces::king, source, target, flags, captured );
}

void calc_moves_king( position const& p, color::type c, move_info*& moves, check_map const& check,
					  unsigned char source, unsigned char target )
{
	pieces::type captured = static_cast<pieces::type>(p.board[target] & 0x0f);
	add_if_legal_king( p, c, moves, source, target, move_flags::valid, captured );
}


void calc_moves_king( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long kings = p.bitboards[c].b[bb_type::king];
	unsigned long long king;
	bitscan(kings, king);

	unsigned long long other_kings = p.bitboards[1-c].b[bb_type::king];
	unsigned long long other_king;
	bitscan(other_kings, other_king);

	unsigned long long king_moves = possible_king_moves[king] & ~(p.bitboards[c].b[bb_type::all_pieces] | possible_king_moves[other_king]) & p.bitboards[1-c].b[bb_type::all_pieces];
	while( king_moves ) {
		unsigned long long i;
		bitscan( king_moves, i );
		king_moves &= king_moves - 1;
		calc_moves_king( p, c, moves, check,
						 king, i );
	}
}


void calc_moves_queen( position const& p, color::type c, move_info*& moves, check_map const& check, unsigned long long queen )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = rook_attacks( queen, all_blockers ) | bishop_attacks( queen, all_blockers );
	possible_moves &= p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long queen_move;
	while( possible_moves ) {
		bitscan( possible_moves, queen_move );
		possible_moves &= possible_moves - 1;

		pieces::type captured = static_cast<pieces::type>(p.board[queen_move] & 0x0f);
		add_if_legal( p, c, moves, check, pieces::queen, queen, queen_move, move_flags::valid, captured );
	}
}


void calc_moves_queens( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long queens = p.bitboards[c].b[bb_type::queens];
	while( queens ) {
		unsigned long long queen;
		bitscan( queens, queen );	
		queens &= queens - 1;
		calc_moves_queen( p, c, moves, check, queen );
	}
}


void calc_moves_bishop( position const& p, color::type c, move_info*& moves, check_map const& check,
					    unsigned long long bishop )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = bishop_attacks( bishop, all_blockers );
	possible_moves &= p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long bishop_move;
	while( possible_moves ) {
		bitscan( possible_moves, bishop_move );
		possible_moves &= possible_moves - 1;

		pieces::type captured = static_cast<pieces::type>(p.board[bishop_move] & 0x0f);
		add_if_legal( p, c, moves, check, pieces::bishop, bishop, bishop_move, move_flags::valid, captured );
	}
}


void calc_moves_bishops( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long bishops = p.bitboards[c].b[bb_type::bishops];
	while( bishops ) {
		unsigned long long bishop;
		bitscan( bishops, bishop );	
		bishops &= bishops - 1;
		calc_moves_bishop( p, c, moves, check, bishop );
	}
}


void calc_moves_rook( position const& p, color::type c, move_info*& moves, check_map const& check,
					  unsigned long long rook )
{
	unsigned long long const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long possible_moves = rook_attacks( rook, all_blockers );
	possible_moves &= p.bitboards[1-c].b[bb_type::all_pieces];

	unsigned long long rook_move;
	while( possible_moves ) {
		bitscan( possible_moves, rook_move );
		possible_moves &= possible_moves - 1;

		pieces::type captured = static_cast<pieces::type>(p.board[rook_move] & 0x0f);
		add_if_legal( p, c, moves, check, pieces::rook, rook, rook_move, move_flags::valid, captured );
	}
}


void calc_moves_rooks( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long rooks = p.bitboards[c].b[bb_type::rooks];
	while( rooks ) {
		unsigned long long rook;
		bitscan( rooks, rook );
		rooks &= rooks - 1;
		calc_moves_rook( p, c, moves, check, rook );
	}
}


void calc_moves_knight( position const& p, color::type c, move_info*& moves, check_map const& check,
						unsigned char source, unsigned char target )
{
	pieces::type captured = static_cast<pieces::type>(p.board[target] & 0x0f);
	add_if_legal( p, c, moves, check, pieces::knight, source, target, move_flags::valid, captured );
}

void calc_moves_knight( position const& p, color::type c, move_info*& moves, check_map const& check,
					    unsigned long long old_knight )
{
	unsigned long long new_knights = possible_knight_moves[old_knight] & ~(p.bitboards[c].b[bb_type::all_pieces]) & p.bitboards[1-c].b[bb_type::all_pieces];
	while( new_knights ) {
		unsigned long long new_knight;
		bitscan( new_knights, new_knight );
		new_knights &= new_knights - 1;
		calc_moves_knight( p, c, moves, check,
						   old_knight, new_knight );
	}
}


void calc_moves_knights( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long knights = p.bitboards[c].b[bb_type::knights];
	while( knights ) {
		unsigned long long knight;
		bitscan( knights, knight );
		knights &= knights - 1;
		calc_moves_knight( p, c, moves, check, knight );
	}
}


void calc_moves_pawn_en_passant( position const& p, color::type c, move_info*& moves, check_map const& check,
								 unsigned long long pawn )
{
	unsigned long long enpassantable = 1ull << p.can_en_passant;
	unsigned long long enpassants = pawn_control[c][pawn] & enpassantable & pawn_enpassant[c];
	if( enpassants ) {
		unsigned long long enpassant;
		bitscan( enpassants, enpassant )

		unsigned char new_col = enpassant % 8;

		unsigned char old_col = static_cast<unsigned char>(pawn % 8);
		unsigned char old_row = static_cast<unsigned char>(pawn / 8);

		// Special case: Cannot use normal check from add_if_legal as target square is not piece square and if captured pawn gives check, bad things happen.
		unsigned char const& cv_old = check.board[pawn];
		unsigned char const& cv_new = check.board[enpassant];
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
		unsigned long long kings = p.bitboards[c].b[bb_type::king];
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
				unsigned char t = p.board[col + old_row * 8];
				if( !t ) {
					// Empty square
					continue;
				}
				if( ( t >> 4) == c ) {
					// Own piece
					break;
				}
				t &= 0x0f;
				if( t == pieces::queen || t == pieces::rook ) {
					// Not a legal move unfortunately
					return;
				}

				// Harmless piece
				break;
			}
		}

		do_add_move( p, c, moves, pieces::pawn, pawn, enpassant, move_flags::valid | move_flags::enpassant, pieces::pawn );
	}
}

void calc_moves_pawn( position const& p, color::type c, move_info*& moves, check_map const& check,
					  unsigned long long pawn )
{
	unsigned long long pawn_captures = pawn_control[c][pawn] & p.bitboards[1-c].b[bb_type::all_pieces];
	while( pawn_captures ) {
		unsigned long long target;
		bitscan( pawn_captures, target );
		pawn_captures &= pawn_captures - 1;

		int flags = move_flags::valid;
		if( target / 8 == ( c ? 0 : 7 ) )  {
			flags |= move_flags::promotion;
		}

		pieces::type captured = static_cast<pieces::type>( p.board[target] & 0x0f );
		add_if_legal( p, c, moves, check, pieces::pawn, pawn, target, flags, captured );
	}

	calc_moves_pawn_en_passant( p, c, moves, check, pawn );
}

void calc_moves_pawns( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	unsigned long long pawns = p.bitboards[c].b[bb_type::pawns];
	while( pawns ) {
		unsigned long long pawn;
		bitscan( pawns, pawn );
		pawns &= pawns - 1;

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
