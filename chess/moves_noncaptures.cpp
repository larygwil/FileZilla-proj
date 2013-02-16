#include "assert.hpp"
#include "moves.hpp"
#include "eval.hpp"
#include "magic.hpp"
#include "util.hpp"
#include "calc.hpp"
#include "tables.hpp"

#include <algorithm>
#include <iostream>
#include <string>

extern uint64_t const pawn_double_move[2];

namespace {

void do_add_move( move_info*& moves,
				  uint64_t const& source, uint64_t const& target,
				  int flags )
{
	ASSERT( source < 64 );
	ASSERT( target < 64 );

	move_info& mi= *(moves++);

	mi.m = move( static_cast<unsigned short>(source), static_cast<unsigned short>(target), flags );
}

// Adds the move if it does not result in self getting into check
void add_if_legal( move_info*& moves, check_map const& check,
				  uint64_t const& source, uint64_t const& target,
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

	do_add_move( moves, source, target, flags );
}

void add_if_legal_pawn( move_info*& moves, check_map const& check,
				  uint64_t const& source, uint64_t const& target )
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
		do_add_move( moves, source, target, move_flags::promotion_queen );
		do_add_move( moves, source, target, move_flags::promotion_rook );
		do_add_move( moves, source, target, move_flags::promotion_bishop );
		do_add_move( moves, source, target, move_flags::promotion_knight );
	}
	else {
		do_add_move( moves, source, target, move_flags::none );
	}
}

void add_if_legal_king( position const& p,
						move_info*& moves, uint64_t const& source, uint64_t const& target,
						int flags )
{
	if( detect_check( p, p.self(), target, source ) ) {
		return;
	}

	do_add_move( moves, source, target, flags );
}

void calc_moves_king( position const& p, move_info*& moves,
					  uint64_t const& source, uint64_t const& target )
{
	add_if_legal_king( p, moves, source, target, move_flags::none );
}


template<bool only_pseudo_checks>
void calc_moves_castles( position const& p, move_info*& moves, check_map const& check )
{
	if( check.check ) {
		return;
	}

	unsigned char row = p.white() ? 0 : 56;
	// Queenside castling
	if( p.castle[p.self()] & 0x2 ) {
		if( p.get_occupancy( 0xeull << row ) == 0 && !(possible_king_moves[2 + row] & p.bitboards[p.other()].b[bb_type::king] ) ) {
			if( !only_pseudo_checks || (p.king_pos[p.other()] % 8) == 3 ) {
				if( !detect_check( p, p.self(), 3 + row, 3 + row ) ) {
					add_if_legal_king( p, moves, 4 + row, 2 + row, move_flags::castle );
				}
			}
		}
	}
	// Kingside castling
	if( p.castle[p.self()] & 0x1 ) {
		if( p.get_occupancy( 0x60ull << row ) == 0 && !(possible_king_moves[6 + row] & p.bitboards[p.other()].b[bb_type::king] ) ) {
			if( !only_pseudo_checks || (p.king_pos[p.other()] % 8) == 5 ) {
				if( !detect_check( p, p.self(), 5 + row, 5 + row ) ) {
					add_if_legal_king( p, moves, 4 + row, 6 + row, move_flags::castle );
				}
			}
		}
	}
}

template<bool only_pseudo_checks>
void calc_moves_king( position const& p, move_info*& moves, check_map const& check )
{
	calc_moves_castles<only_pseudo_checks>( p, moves, check );

	if( only_pseudo_checks ) {
		uint64_t occ = p.bitboards[p.self()].b[bb_type::all_pieces] | p.bitboards[p.other()].b[bb_type::all_pieces];
		occ ^= p.bitboards[p.self()].b[bb_type::king];

		uint64_t ba = bishop_magic( p.king_pos[p.other()], occ ) & (p.bitboards[p.self()].b[bb_type::bishops] | p.bitboards[p.self()].b[bb_type::queens]);
		uint64_t ra = rook_magic( p.king_pos[p.other()], occ ) & (p.bitboards[p.self()].b[bb_type::rooks] | p.bitboards[p.self()].b[bb_type::queens]);
		if( !(ba | ra) ) {
			return;
		}
	}

	uint64_t king_moves = possible_king_moves[p.king_pos[p.self()]] & ~(p.bitboards[p.self()].b[bb_type::all_pieces] | p.bitboards[p.other()].b[bb_type::all_pieces] | possible_king_moves[p.king_pos[p.other()]]);
	while( king_moves ) {
		uint64_t king_move = bitscan_unset( king_moves );
		calc_moves_king( p, moves,
						 p.king_pos[p.self()], king_move );
	}
}


template<bool only_pseudo_checks>
void calc_moves_queen( position const& p, move_info*& moves, check_map const& check, uint64_t queen )
{
	uint64_t const all_blockers = p.bitboards[p.self()].b[bb_type::all_pieces] | p.bitboards[p.other()].b[bb_type::all_pieces];

	uint64_t possible_moves = rook_magic( queen, all_blockers ) | bishop_magic( queen, all_blockers );
	possible_moves &= ~(p.bitboards[p.self()].b[bb_type::all_pieces] | p.bitboards[p.other()].b[bb_type::all_pieces]);

	if( only_pseudo_checks ) {
		uint64_t occ = all_blockers ^ (1ull << queen);
		uint64_t ba = bishop_magic( p.king_pos[p.other()], occ ) & (p.bitboards[p.self()].b[bb_type::bishops] | p.bitboards[p.self()].b[bb_type::queens]);
		uint64_t ra = rook_magic( p.king_pos[p.other()], occ ) & (p.bitboards[p.self()].b[bb_type::rooks] | p.bitboards[p.self()].b[bb_type::queens]);
		if( !(ba | ra ) ) {
			possible_moves &= bishop_magic( p.king_pos[p.other()], all_blockers ) | rook_magic( p.king_pos[p.other()], all_blockers );
		}
	}

	while( possible_moves ) {
		uint64_t queen_move = bitscan_unset( possible_moves );
		add_if_legal( moves, check, queen, queen_move, move_flags::none );
	}
}


template<bool only_pseudo_checks>
void calc_moves_queens( position const& p, move_info*& moves, check_map const& check )
{
	uint64_t queens = p.bitboards[p.self()].b[bb_type::queens];
	while( queens ) {
		uint64_t queen = bitscan_unset( queens );
		calc_moves_queen<only_pseudo_checks>( p, moves, check, queen );
	}
}


template<bool only_pseudo_checks>
void calc_moves_bishop( position const& p, move_info*& moves, check_map const& check,
						uint64_t bishop )
{
	uint64_t const all_blockers = p.bitboards[p.self()].b[bb_type::all_pieces] | p.bitboards[p.other()].b[bb_type::all_pieces];

	uint64_t possible_moves = bishop_magic( bishop, all_blockers );
	possible_moves &= ~(p.bitboards[p.self()].b[bb_type::all_pieces] | p.bitboards[p.other()].b[bb_type::all_pieces]);

	if( only_pseudo_checks ) {
		uint64_t occ = all_blockers ^ (1ull << bishop);
		uint64_t ba = bishop_magic( p.king_pos[p.other()], occ ) & (p.bitboards[p.self()].b[bb_type::bishops] | p.bitboards[p.self()].b[bb_type::queens]);
		uint64_t ra = rook_magic( p.king_pos[p.other()], occ ) & (p.bitboards[p.self()].b[bb_type::rooks] | p.bitboards[p.self()].b[bb_type::queens]);
		if( !(ba | ra ) ) {
			possible_moves &= bishop_magic( p.king_pos[p.other()], all_blockers );
		}
	}

	while( possible_moves ) {
		uint64_t bishop_move = bitscan_unset( possible_moves );
		add_if_legal( moves, check, bishop, bishop_move, move_flags::none );
	}
}


template<bool only_pseudo_checks>
void calc_moves_bishops( position const& p, move_info*& moves, check_map const& check )
{
	uint64_t bishops = p.bitboards[p.self()].b[bb_type::bishops];
	while( bishops ) {
		uint64_t bishop = bitscan_unset( bishops );
		calc_moves_bishop<only_pseudo_checks>( p, moves, check, bishop );
	}
}


template<bool only_pseudo_checks>
void calc_moves_rook( position const& p, move_info*& moves, check_map const& check,
					  uint64_t rook )
{
	uint64_t const all_blockers = p.bitboards[p.self()].b[bb_type::all_pieces] | p.bitboards[p.other()].b[bb_type::all_pieces];

	uint64_t possible_moves = rook_magic( rook, all_blockers );
	possible_moves &= ~(p.bitboards[p.self()].b[bb_type::all_pieces] | p.bitboards[p.other()].b[bb_type::all_pieces]);

	if( only_pseudo_checks ) {
		uint64_t occ = all_blockers ^ (1ull << rook);
		uint64_t ba = bishop_magic( p.king_pos[p.other()], occ ) & (p.bitboards[p.self()].b[bb_type::bishops] | p.bitboards[p.self()].b[bb_type::queens]);
		uint64_t ra = rook_magic( p.king_pos[p.other()], occ ) & (p.bitboards[p.self()].b[bb_type::rooks] | p.bitboards[p.self()].b[bb_type::queens]);
		if( !(ba | ra ) ) {
			possible_moves &= rook_magic( p.king_pos[p.other()], all_blockers );
		}
	}

	while( possible_moves ) {
		uint64_t rook_move = bitscan_unset( possible_moves);
		add_if_legal( moves, check, rook, rook_move, move_flags::none );
	}
}


template<bool only_pseudo_checks>
void calc_moves_rooks( position const& p, move_info*& moves, check_map const& check )
{
	uint64_t rooks = p.bitboards[p.self()].b[bb_type::rooks];
	while( rooks ) {
		uint64_t rook = bitscan_unset( rooks );
		calc_moves_rook<only_pseudo_checks>( p, moves, check, rook );
	}
}


template<bool only_pseudo_checks>
void calc_moves_knight( position const& p, move_info*& moves, check_map const& check,
						uint64_t old_knight )
{
	uint64_t new_knights = possible_knight_moves[old_knight] & ~(p.bitboards[p.self()].b[bb_type::all_pieces] | p.bitboards[p.other()].b[bb_type::all_pieces]);

	if( only_pseudo_checks ) {
		uint64_t const all_blockers = p.bitboards[p.self()].b[bb_type::all_pieces] | p.bitboards[p.other()].b[bb_type::all_pieces];
		uint64_t occ = all_blockers ^ (1ull << old_knight);
		uint64_t ba = bishop_magic( p.king_pos[p.other()], occ ) & (p.bitboards[p.self()].b[bb_type::bishops] | p.bitboards[p.self()].b[bb_type::queens]);
		uint64_t ra = rook_magic( p.king_pos[p.other()], occ ) & (p.bitboards[p.self()].b[bb_type::rooks] | p.bitboards[p.self()].b[bb_type::queens]);
		if( !(ba | ra ) ) {
			new_knights &= possible_knight_moves[p.king_pos[p.other()]];
		}
	}

	while( new_knights ) {
		uint64_t new_knight = bitscan_unset( new_knights );
		add_if_legal( moves, check, old_knight, new_knight, move_flags::none );
	}
}


template<bool only_pseudo_checks>
void calc_moves_knights( position const& p, move_info*& moves, check_map const& check )
{
	uint64_t knights = p.bitboards[p.self()].b[bb_type::knights];
	while( knights ) {
		uint64_t knight = bitscan_unset( knights );
		calc_moves_knight<only_pseudo_checks>( p, moves, check, knight );
	}
}


template<int c, bool only_pseudo_checks>
void calc_moves_pawn_pushes( position const& p, move_info*& moves, check_map const& check )
{
	uint64_t blockers = p.bitboards[p.self()].b[bb_type::all_pieces] | p.bitboards[p.other()].b[bb_type::all_pieces];
	uint64_t free = ~blockers;

	uint64_t pawn_pushes;
	uint64_t double_pushes;

	if( c == color::white ) {
		pawn_pushes = (p.bitboards[p.self()].b[bb_type::pawns] << 8) & free;
		double_pushes = ((pawn_pushes & pawn_double_move[p.self()]) << 8) & free;

		if( only_pseudo_checks ) {
			uint64_t checks = rook_magic( p.king_pos[p.other()], blockers ) | bishop_magic( p.king_pos[p.other()], blockers );
			pawn_pushes &= (checks << 8) | pawn_control[p.other()][p.king_pos[p.other()]] | 0xff000000000000ffull;
			double_pushes &= (checks << 16) | pawn_control[p.other()][p.king_pos[p.other()]] | 0xff000000000000ffull;
		}
	}
	else {
		pawn_pushes = (p.bitboards[p.self()].b[bb_type::pawns] >> 8) & free;
		double_pushes = ((pawn_pushes & pawn_double_move[p.self()]) >> 8) & free;

		if( only_pseudo_checks ) {
			uint64_t checks = rook_magic( p.king_pos[p.other()], blockers ) | bishop_magic( p.king_pos[p.other()], blockers );
			pawn_pushes &= (checks >> 8) | pawn_control[p.other()][p.king_pos[p.other()]] | 0xff000000000000ffull;
			double_pushes &= (checks >> 16) | pawn_control[p.other()][p.king_pos[p.other()]] | 0xff000000000000ffull;
		}
	}
	while( double_pushes ) {
		uint64_t pawn_move = bitscan_unset( double_pushes );
		add_if_legal( moves, check, pawn_move - (c ? -16 : 16), pawn_move, move_flags::none );
	}

	while( pawn_pushes ) {
		uint64_t pawn_move = bitscan_unset( pawn_pushes );

		add_if_legal_pawn( moves, check, pawn_move - (c ? -8 : 8), pawn_move );
	}
}


template<bool only_pseudo_checks>
void calc_moves_pawns( position const& p, move_info*& moves, check_map const& check )
{

	if( p.white() ) {
		calc_moves_pawn_pushes<color::white, only_pseudo_checks>( p, moves, check );
	}
	else {
		calc_moves_pawn_pushes<color::black, only_pseudo_checks>( p, moves, check );
	}
}
}

template<bool only_pseudo_checks>
void calculate_moves_noncaptures( position const& p, move_info*& moves, check_map const& check )
{
	if( !check.check || !check.multiple() )	{
		calc_moves_pawns<only_pseudo_checks>( p, moves, check );
		calc_moves_queens<only_pseudo_checks>( p, moves, check );
		calc_moves_rooks<only_pseudo_checks>( p, moves, check );
		calc_moves_bishops<only_pseudo_checks>( p, moves, check );
		calc_moves_knights<only_pseudo_checks>( p, moves, check );
	}
	calc_moves_king<only_pseudo_checks>( p, moves, check );
}


template void calculate_moves_noncaptures<true>( position const& p, move_info*& moves, check_map const& check );
template void calculate_moves_noncaptures<false>( position const& p, move_info*& moves, check_map const& check );
