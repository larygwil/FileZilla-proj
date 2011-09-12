#include "mobility.hpp"
#include "mobility_data.hpp"
#include <iostream>

extern unsigned long long const possible_knight_moves[64];

namespace {

namespace pin_values {
enum type {
	absolute_bishop = 35,
	absolute_rook = 30,
	absolute_queen = 25
};
};


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


inline static void evaluate_knights_mobility( position const& p, color::type c, bitboard const* bitboards, short& mobility )
{
	unsigned long long knights = bitboards[c].knights;

	unsigned long long knight;
	while( knights ) {
		bitscan( knights, knight );
		knights ^= 1ull << knight;

		unsigned long long moves = possible_knight_moves[knight];
		moves &= ~bitboards[c].all_pieces;
		moves &= ~bitboards[1-c].pawn_control;

		short kev = static_cast<short>(popcount(moves));

		mobility += knight_mobility_scale[kev];
	}

}


inline static void evaluate_bishop_mobility( position const& p, color::type c, bitboard const* bitboards, unsigned long long bishop, unsigned long long moves, short& mobility )
{
	unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns | bitboards[c].king;
	blockers &= moves;

	moves &= ~bitboards[c].all_pieces;
	moves &= ~bitboards[1-c].pawn_control;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[bishop][blocker];
		moves &= ~mobility_block[bishop][blocker];
	}

	short bev = static_cast<short>(popcount(moves));

	mobility += bishop_mobility_scale[bev];
}


inline static void evaluate_bishop_pin( position const& p, color::type c, bitboard const* bitboards, unsigned long long bishop, unsigned long long moves, short& pin )
{
	unsigned long long blockers = bitboards[c].all_pieces | bitboards[1-c].pawns;
	blockers &= moves;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[bishop][blocker];
		moves &= ~mobility_block[bishop][blocker];
	}

	unsigned long long unblocked_moves = moves;

	blockers = bitboards[1-c].all_pieces;
	blockers &= moves;

	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[bishop][blocker];
		moves &= ~mobility_block[bishop][blocker];
	}

	// Absolute pin
	if( (moves ^ unblocked_moves) & bitboards[1-c].king ) {
		pin += pin_values::absolute_bishop;
	}
}


inline static void evaluate_bishops_mobility( position const& p, color::type c, bitboard const* bitboards, short& mobility, short& pin )
{
	unsigned long long bishops = bitboards[c].bishops;

	unsigned long long bishop;
	while( bishops ) {
		bitscan( bishops, bishop );
		bishops ^= 1ull << bishop;

		unsigned long long moves = visibility_bishop[bishop];
		evaluate_bishop_mobility( p, c, bitboards, bishop, moves, mobility );
		evaluate_bishop_pin( p, c, bitboards, bishop, moves, pin );
	}
}


inline static void evaluate_rook_mobility( position const& p, color::type c, bitboard const* bitboards, unsigned long long rook, unsigned long long moves, short& mobility )
{
	unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns | bitboards[c].king;
	blockers &= moves;

	moves &= ~bitboards[c].all_pieces;
	moves &= ~bitboards[1-c].pawn_control;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[rook][blocker];
		moves &= ~mobility_block[rook][blocker];
	}

	unsigned long long vertical = moves & (0x0101010101010101ull << (rook % 8));
	unsigned long long horizontal = moves & ~vertical;

	// Rooks dig verticals.
	short rev = static_cast<short>(popcount(horizontal)) + 2 * static_cast<short>(popcount(vertical));

	// Punish bad mobility
	mobility += rook_mobility_scale[rev];

}


inline static void evaluate_rook_pin( position const& p, color::type c, bitboard const* bitboards, unsigned long long rook, unsigned long long moves, short& pin )
{
	unsigned long long blockers = bitboards[c].all_pieces | bitboards[1-c].pawns;
	blockers &= moves;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[rook][blocker];
		moves &= ~mobility_block[rook][blocker];
	}

	unsigned long long unblocked_moves = moves;

	blockers = bitboards[1-c].all_pieces;
	blockers &= moves;

	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[rook][blocker];
		moves &= ~mobility_block[rook][blocker];
	}

	// Absolute pin
	if( (moves ^ unblocked_moves) & bitboards[1-c].king ) {
		pin += pin_values::absolute_rook;
	}
}


inline static void evaluate_rooks_mobility( position const& p, color::type c, bitboard const* bitboards, short& mobility, short& pin )
{
	unsigned long long rooks = bitboards[c].rooks;

	unsigned long long rook;
	while( rooks ) {
		bitscan( rooks, rook );
		rooks ^= 1ull << rook;

		unsigned long long moves = visibility_rook[rook];
		evaluate_rook_mobility( p, c, bitboards, rook, moves, mobility );
		evaluate_rook_pin( p, c, bitboards, rook, moves, pin );
	}
}


inline static void evaluate_queen_mobility( position const& p, color::type c, bitboard const* bitboards, unsigned long long queen, unsigned long long moves, short& mobility )
{
	unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns | bitboards[c].king;
	blockers &= moves;

	moves &= ~bitboards[c].all_pieces;
	moves &= ~bitboards[1-c].pawn_control;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[queen][blocker];
		moves &= ~mobility_block[queen][blocker];
	}

	// Queen is just too mobile, can get anywhere quickly. Just punish very low mobility.
	short qev = static_cast<short>(popcount(moves));
	mobility += (qev > 5) ? 5 : qev;
}


inline static void evaluate_queen_pin( position const& p, color::type c, bitboard const* bitboards, unsigned long long queen, unsigned long long moves, short& pin )
{
	unsigned long long blockers = bitboards[c].all_pieces | bitboards[1-c].pawns;
	blockers &= moves;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[queen][blocker];
		moves &= ~mobility_block[queen][blocker];
	}

	unsigned long long unblocked_moves = moves;

	blockers = bitboards[1-c].all_pieces;
	blockers &= moves;

	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[queen][blocker];
		moves &= ~mobility_block[queen][blocker];
	}

	// Absolute pin
	if( (moves ^ unblocked_moves) & bitboards[1-c].king ) {
		pin += pin_values::absolute_queen;
	}
}


inline static void evaluate_queens_mobility( position const& p, color::type c, bitboard const* bitboards, short& mobility, short& pin )
{
	unsigned long long queens = bitboards[c].queens;

	unsigned long long queen;
	while( queens ) {
		bitscan( queens, queen );
		queens ^= 1ull << queen;

		unsigned long long moves = visibility_bishop[queen] | visibility_rook[queen];
		evaluate_queen_mobility( p, c, bitboards, queen, moves, mobility );
		evaluate_queen_pin( p, c, bitboards, queen, moves, pin );
	}
}
}

void evaluate_mobility( position const& p, color::type c, bitboard const* bitboards, short& mobility, short& pin )
{
	short mobility_self = 0;
	short pin_self = 0;
	evaluate_knights_mobility( p, c, bitboards, mobility_self );
	evaluate_bishops_mobility( p, c, bitboards, mobility_self, pin_self );
	evaluate_rooks_mobility( p, c, bitboards, mobility_self, pin_self );
	evaluate_queens_mobility( p, c, bitboards, mobility_self, pin_self );

	mobility += mobility_self;
	pin += pin_self;

	short mobility_other = 0;
	short pin_other = 0;
	evaluate_knights_mobility( p, static_cast<color::type>(1-c), bitboards, mobility_other );
	evaluate_bishops_mobility( p, static_cast<color::type>(1-c), bitboards, mobility_other, pin_other );
	evaluate_rooks_mobility( p, static_cast<color::type>(1-c), bitboards, mobility_other, pin_other );
	evaluate_queens_mobility( p, static_cast<color::type>(1-c), bitboards, mobility_other, pin_other );

	mobility -= mobility_other;
	pin -= pin_other;
}
