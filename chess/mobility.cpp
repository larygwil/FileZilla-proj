#include "mobility.hpp"
#include "mobility_data.hpp"
#include <iostream>
extern unsigned long long const possible_knight_moves[64];


short get_knight_mobility( position const& p, bitboard const* bitboards, color::type c )
{
	short ev = 0;

	unsigned long long knights = bitboards[c].knights;

	unsigned long long knight;
	while( (knight = __builtin_ffsll(knights) ) ) {
		--knight;
		knights ^= 1ull << knight;

		unsigned long long moves = possible_knight_moves[knight];
		moves &= ~bitboards[c].all_pieces;
		moves &= ~bitboards[1-c].pawn_control;

		ev += __builtin_popcountll(moves);
	}

	return ev;
}


short get_bishop_mobility( position const& p, bitboard const* bitboards, color::type c )
{
	short ev = 0;

	unsigned long long bishops = bitboards[c].bishops;

	unsigned long long bishop;
	while( (bishop = __builtin_ffsll(bishops) ) ) {
		--bishop;
		bishops ^= 1ull << bishop;

		unsigned long long moves = visibility_bishop[bishop];


		unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns;
		blockers &= moves;

		moves &= ~bitboards[c].all_pieces;
		moves &= ~bitboards[1-c].pawn_control;

		unsigned long long blocker;
		while( (blocker = __builtin_ffsll(blockers) ) ) {
			--blocker;
			blockers ^= 1ull << blocker;

			blockers &= ~mobility_block[bishop][blocker];
			moves &= ~mobility_block[bishop][blocker];
		}

		ev += __builtin_popcountll(moves);
	}

	return ev;
}


short get_rook_mobility( position const& p, bitboard const* bitboards, color::type c )
{
	short ev = 0;

	unsigned long long rooks = bitboards[c].rooks;

	unsigned long long rook;
	while( (rook = __builtin_ffsll(rooks) ) ) {
		--rook;
		rooks ^= 1ull << rook;

		unsigned long long moves = visibility_rook[rook];


		unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns;
		blockers &= moves;

		moves &= ~bitboards[c].all_pieces;
		moves &= ~bitboards[1-c].pawn_control;

		unsigned long long blocker;
		while( (blocker = __builtin_ffsll(blockers) ) ) {
			--blocker;
			blockers ^= 1ull << blocker;

			blockers &= ~mobility_block[rook][blocker];
			moves &= ~mobility_block[rook][blocker];
		}

		ev += __builtin_popcountll(moves);
	}

	return ev;
}


short get_queen_mobility( position const& p, bitboard const* bitboards, color::type c )
{
	short ev = 0;

	unsigned long long queens = bitboards[c].queens;

	unsigned long long queen;
	while( (queen = __builtin_ffsll(queens) ) ) {
		--queen;
		queens ^= 1ull << queen;

		unsigned long long moves = visibility_bishop[queen] | visibility_rook[queen];


		unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns;
		blockers &= moves;

		moves &= ~bitboards[c].all_pieces;
		moves &= ~bitboards[1-c].pawn_control;

		unsigned long long blocker;
		while( (blocker = __builtin_ffsll(blockers) ) ) {
			--blocker;
			blockers ^= 1ull << blocker;

			blockers &= ~mobility_block[queen][blocker];
			moves &= ~mobility_block[queen][blocker];
		}

		ev += __builtin_popcountll(moves);
	}

	// Queen is just too mobile to leave it unvalued
	return ev / 4;
}


short get_mobility( position const& p, bitboard const* bitboards, color::type c )
{
	short ev = get_knight_mobility( p, bitboards, c )
			+ get_bishop_mobility( p, bitboards, c );
			+ get_rook_mobility( p, bitboards, c );
			+ get_queen_mobility( p, bitboards, c );

	return ev;
}


short get_mobility( position const& p, bitboard const* bitboards )
{
	short ev = get_mobility( p, bitboards, color::white ) - get_mobility( p, bitboards, color::black );

	return ev;
}
