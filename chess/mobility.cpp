#include "mobility.hpp"
#include "mobility_data.hpp"
#include <iostream>
extern unsigned long long const possible_knight_moves[64];

// Using the XRAYS pattern, own pieces do not stop mobility.
// We do stop at own pawns and king though.
// Naturally, enemy pieces also block us.

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

		short kev = __builtin_popcountll(moves);

		// Punish bad mobility
		if( kev < 3 ) {
			--kev;
		}

		ev += kev;
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

		unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns | bitboards[c].king;
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

		short bev = __builtin_popcountll(moves);

		// Punish bad mobility
		if( bev < 5 ) {
			--bev;
		}

		ev += bev;
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


		unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns | bitboards[c].king;
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

		unsigned long long vertical = moves & (0x0101010101010101ull << (rook % 8));
		unsigned long long horizontal = moves & ~vertical;

		// Rooks dig verticals.
		short rev = __builtin_popcountll(horizontal) + 2 * __builtin_popcountll(vertical);

		// Punish bad mobility
		if( rev < 5 ) {
			--rev;
		}


		ev += rev;
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


		unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns | bitboards[c].king;
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

		// Queen is just too mobile, can get anywhere quickly. Just punish very low mobility.
		short qev = __builtin_popcountll(moves);
		ev += (qev > 5) ? 5 : qev;
	}

	return ev;
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
