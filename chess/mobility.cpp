#include "mobility.hpp"
#include "mobility_data.hpp"
#include <iostream>

namespace pin_values {
enum type {
	absolute_bishop = 35,
	absolute_rook = 30,
	absolute_queen = 25
};
};

extern unsigned long long const possible_knight_moves[64];

// Using the XRAYS pattern, own pieces do not stop mobility.
// We do stop at own pawns and king though.
// Naturally, enemy pieces also block us.

// Punish bad mobility, reward great mobility
short knight_mobility_scale[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8
};
short bishop_mobility_scale[] = {
	-1, 0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
};
short rook_mobility_scale[] = {
	-1, 0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
};
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

		ev += knight_mobility_scale[kev];
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

		ev += bishop_mobility_scale[bev];
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
		ev += rook_mobility_scale[rev];
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
			+ get_bishop_mobility( p, bitboards, c )
			+ get_rook_mobility( p, bitboards, c )
			+ get_queen_mobility( p, bitboards, c );

	return ev;
}


short get_mobility( position const& p, bitboard const* bitboards )
{
	short ev = get_mobility( p, bitboards, color::white ) - get_mobility( p, bitboards, color::black );

	return ev;
}


short get_bishop_pins( position const& p, bitboard const* bitboards, color::type c )
{
	short ev = 0;

	unsigned long long bishops = bitboards[c].bishops;

	unsigned long long bishop;
	while( (bishop = __builtin_ffsll(bishops) ) ) {
		--bishop;
		bishops ^= 1ull << bishop;

		unsigned long long moves = visibility_bishop[bishop];

		unsigned long long blockers = bitboards[c].all_pieces | bitboards[1-c].pawns;
		blockers &= moves;

		unsigned long long blocker;
		while( (blocker = __builtin_ffsll(blockers) ) ) {
			--blocker;
			blockers ^= 1ull << blocker;

			blockers &= ~mobility_block[bishop][blocker];
			moves &= ~mobility_block[bishop][blocker];
		}

		unsigned long long unblocked_moves = moves;

		blockers = bitboards[1-c].all_pieces;
		blockers &= moves;

		while( (blocker = __builtin_ffsll(blockers) ) ) {
			--blocker;
			blockers ^= 1ull << blocker;

			blockers &= ~mobility_block[bishop][blocker];
			moves &= ~mobility_block[bishop][blocker];
		}

		// Absolute pin
		if( (moves ^ unblocked_moves) & bitboards[1-c].king ) {
			ev += pin_values::absolute_bishop;
		}
	}

	return ev;
}


short get_rook_pins( position const& p, bitboard const* bitboards, color::type c )
{
	short ev = 0;

	unsigned long long rooks = bitboards[c].rooks;

	unsigned long long rook;
	while( (rook = __builtin_ffsll(rooks) ) ) {
		--rook;
		rooks ^= 1ull << rook;

		unsigned long long moves = visibility_rook[rook];

		unsigned long long blockers = bitboards[c].all_pieces | bitboards[1-c].pawns;
		blockers &= moves;

		unsigned long long blocker;
		while( (blocker = __builtin_ffsll(blockers) ) ) {
			--blocker;
			blockers ^= 1ull << blocker;

			blockers &= ~mobility_block[rook][blocker];
			moves &= ~mobility_block[rook][blocker];
		}

		unsigned long long unblocked_moves = moves;

		blockers = bitboards[1-c].all_pieces;
		blockers &= moves;

		while( (blocker = __builtin_ffsll(blockers) ) ) {
			--blocker;
			blockers ^= 1ull << blocker;

			blockers &= ~mobility_block[rook][blocker];
			moves &= ~mobility_block[rook][blocker];
		}

		// Absolute pin
		if( (moves ^ unblocked_moves) & bitboards[1-c].king ) {
			ev += pin_values::absolute_rook;
		}
	}

	return ev;
}


short get_queen_pins( position const& p, bitboard const* bitboards, color::type c )
{
	short ev = 0;

	unsigned long long queens = bitboards[c].queens;

	unsigned long long queen;
	while( (queen = __builtin_ffsll(queens) ) ) {
		--queen;
		queens ^= 1ull << queen;

		unsigned long long moves = visibility_bishop[queen] | visibility_rook[queen];

		unsigned long long blockers = bitboards[c].all_pieces | bitboards[1-c].pawns;
		blockers &= moves;

		unsigned long long blocker;
		while( (blocker = __builtin_ffsll(blockers) ) ) {
			--blocker;
			blockers ^= 1ull << blocker;

			blockers &= ~mobility_block[queen][blocker];
			moves &= ~mobility_block[queen][blocker];
		}

		unsigned long long unblocked_moves = moves;

		blockers = bitboards[1-c].all_pieces;
		blockers &= moves;

		while( (blocker = __builtin_ffsll(blockers) ) ) {
			--blocker;
			blockers ^= 1ull << blocker;

			blockers &= ~mobility_block[queen][blocker];
			moves &= ~mobility_block[queen][blocker];
		}

		// Absolute pin
		if( (moves ^ unblocked_moves) & bitboards[1-c].king ) {
			ev += pin_values::absolute_queen;
		}
	}

	return ev;
}


short get_pins( position const& p, bitboard const* bitboards, color::type c )
{
	short ev = get_bishop_pins( p, bitboards, c )
			+ get_rook_mobility( p, bitboards, c )
			+ get_queen_mobility( p, bitboards, c );

	return ev;
}


short get_pins( position const& p, bitboard const* bitboards )
{
	short ev = get_pins( p, bitboards, color::white ) - get_pins( p, bitboards, color::black );

	return ev;
}
