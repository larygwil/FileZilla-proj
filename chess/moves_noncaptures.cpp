#include "assert.hpp"
#include "moves.hpp"
#include "eval.hpp"
#include "magic.hpp"
#include "util.hpp"
#include "calc.hpp"
#include "sliding_piece_attacks.hpp"
#include "tables.hpp"

#include <algorithm>
#include <iostream>
#include <string>

extern uint64_t const pawn_double_move[2];

namespace {

void do_add_move( move_info*& moves, pieces::type const& pi,
				  unsigned char const& source, unsigned char const& target,
				  int flags )
{
	move_info& mi= *(moves++);

	mi.m.flags = flags;
	mi.m.piece = pi;
	mi.m.source = source;
	mi.m.target = target;
	mi.m.captured_piece = pieces::none;
}

// Adds the move if it does not result in self getting into check
void add_if_legal( move_info*& moves, check_map const& check,
				  pieces::type const& pi,
				  unsigned char const& source, unsigned char const& target,
				  int flags )
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

	do_add_move( moves, pi, source, target, flags );
}

void add_if_legal_pawn( move_info*& moves, check_map const& check,
				  unsigned char const& source, unsigned char const& target )
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

	if( target >= 56 || target < 8 ) {
		do_add_move( moves, pieces::pawn, source, target, move_flags::promotion_queen );
		do_add_move( moves, pieces::pawn, source, target, move_flags::promotion_rook );
		do_add_move( moves, pieces::pawn, source, target, move_flags::promotion_bishop );
		do_add_move( moves, pieces::pawn, source, target, move_flags::promotion_knight );
	}
	else {
		do_add_move( moves, pieces::pawn, source, target, move_flags::none );
	}
}

void add_if_legal_king( position const& p, color::type c,
						move_info*& moves, unsigned char const& source, unsigned char const& target,
						int flags )
{
	if( detect_check( p, c, target, source ) ) {
		return;
	}

	do_add_move( moves, pieces::king, source, target, flags );
}

void calc_moves_king( position const& p, color::type c, move_info*& moves,
					  unsigned char const& source, unsigned char const& target )
{
	add_if_legal_king( p, c, moves, source, target, move_flags::none );
}


void calc_moves_king( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	uint64_t kings = p.bitboards[c].b[bb_type::king];
	uint64_t king = bitscan( kings );

	uint64_t other_kings = p.bitboards[1-c].b[bb_type::king];
	uint64_t other_king = bitscan( other_kings );

	uint64_t king_moves = possible_king_moves[king] & ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces] | possible_king_moves[other_king]);
	while( king_moves ) {
		uint64_t king_move = bitscan_unset( king_moves );
		calc_moves_king( p, c, moves,
						 king, king_move );
	}

	if( check.check ) {
		return;
	}

	unsigned char row = c ? 56 : 0;
	// Queenside castling
	if( p.castle[c] & 0x2 ) {
		if( p.get_occupancy( 0xeull << row ) == 0 && !(possible_king_moves[2 + row] & other_kings ) ) {
			if( !detect_check( p, c, 3 + row, 3 + row ) ) {
				add_if_legal_king( p, c, moves, 4 + row, 2 + row, move_flags::castle );
			}
		}
	}
	// Kingside castling
	if( p.castle[c] & 0x1 ) {
		if( p.get_occupancy( 0x60ull << row ) == 0 && !(possible_king_moves[6 + row] & other_kings ) ) {
			if( !detect_check( p, c, 5 + row, 5 + row ) ) {
				add_if_legal_king( p, c, moves, 4 + row, 6 + row, move_flags::castle );
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, move_info*& moves, check_map const& check, uint64_t queen )
{
	uint64_t const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	uint64_t possible_moves = rook_magic( queen, all_blockers ) | bishop_magic( queen, all_blockers );
	possible_moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]);

	while( possible_moves ) {
		uint64_t queen_move = bitscan_unset( possible_moves );
		add_if_legal( moves, check, pieces::queen, queen, queen_move, move_flags::none );
	}
}


void calc_moves_queens( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	uint64_t queens = p.bitboards[c].b[bb_type::queens];
	while( queens ) {
		uint64_t queen = bitscan_unset( queens );
		calc_moves_queen( p, c, moves, check, queen );
	}
}


void calc_moves_bishop( position const& p, color::type c, move_info*& moves, check_map const& check,
						uint64_t bishop )
{
	uint64_t const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	uint64_t possible_moves = bishop_magic( bishop, all_blockers );
	possible_moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]);

	while( possible_moves ) {
		uint64_t bishop_move = bitscan_unset( possible_moves );
		add_if_legal( moves, check, pieces::bishop, bishop, bishop_move, move_flags::none );
	}
}


void calc_moves_bishops( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	uint64_t bishops = p.bitboards[c].b[bb_type::bishops];
	while( bishops ) {
		uint64_t bishop = bitscan_unset( bishops );
		calc_moves_bishop( p, c, moves, check, bishop );
	}
}


void calc_moves_rook( position const& p, color::type c, move_info*& moves, check_map const& check,
					  uint64_t rook )
{
	uint64_t const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];

	uint64_t possible_moves = rook_magic( rook, all_blockers );
	possible_moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]);

	while( possible_moves ) {
		uint64_t rook_move = bitscan_unset( possible_moves);
		add_if_legal( moves, check, pieces::rook, rook, rook_move, move_flags::none );
	}
}


void calc_moves_rooks( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	uint64_t rooks = p.bitboards[c].b[bb_type::rooks];
	while( rooks ) {
		uint64_t rook = bitscan_unset( rooks );
		calc_moves_rook( p, c, moves, check, rook );
	}
}


void calc_moves_knight( position const& p, color::type c, move_info*& moves, check_map const& check,
						uint64_t old_knight )
{
	uint64_t new_knights = possible_knight_moves[old_knight] & ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]);
	while( new_knights ) {
		uint64_t new_knight = bitscan_unset( new_knights );
		add_if_legal( moves, check, pieces::knight, old_knight, new_knight, move_flags::none );
	}
}


void calc_moves_knights( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	uint64_t knights = p.bitboards[c].b[bb_type::knights];
	while( knights ) {
		uint64_t knight = bitscan_unset( knights );
		calc_moves_knight( p, c, moves, check, knight );
	}
}


template<int c>
void calc_moves_pawn_pushes( position const& p, move_info*& moves, check_map const& check )
{
	uint64_t free = ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]);

	uint64_t pawn_pushes;
	uint64_t double_pushes;
	if( c == color::white ) {
		pawn_pushes = (p.bitboards[c].b[bb_type::pawns] << 8) & free;
		double_pushes= ((pawn_pushes & pawn_double_move[c]) << 8) & free;
	}
	else {
		pawn_pushes = (p.bitboards[c].b[bb_type::pawns] >> 8) & free;
		double_pushes= ((pawn_pushes & pawn_double_move[c]) >> 8) & free;
	}
	while( double_pushes ) {
		uint64_t pawn_move = bitscan_unset( double_pushes );
		add_if_legal( moves, check, pieces::pawn, pawn_move - (c ? -16 : 16), pawn_move, move_flags::pawn_double_move );
	}

	while( pawn_pushes ) {
		uint64_t pawn_move = bitscan_unset( pawn_pushes );

		add_if_legal_pawn( moves, check, pawn_move - (c ? -8 : 8), pawn_move );
	}
}


void calc_moves_pawns( position const& p, color::type c, move_info*& moves, check_map const& check )
{

	if( c == color::white ) {
		calc_moves_pawn_pushes<color::white>( p, moves, check );
	}
	else {
		calc_moves_pawn_pushes<color::black>( p, moves, check );
	}
}
}

void calculate_moves_noncaptures( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	calc_moves_king( p, c, moves, check );

	if( !check.check || !check.multiple() )
	{
		calc_moves_pawns( p, c, moves, check );
		calc_moves_queens( p, c, moves, check );
		calc_moves_rooks( p, c, moves, check );
		calc_moves_bishops( p, c, moves, check );
		calc_moves_knights( p, c, moves, check );
	}
}
