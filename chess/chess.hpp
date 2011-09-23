#ifndef __CHECK_H__
#define __CHECK_H__

#include <string.h>

#include "platform.hpp"

namespace pieces2 {
enum type {
	none,
	pawn,
	knight,
	bishop,
	rook,
	queen,
	king
};
}


namespace promotions {
enum type {
	queen,
	rook,
	knight,
	bishop
};
}


namespace color {
enum type {
	white = 0,
	black = 1
};
}


namespace bb_type {
enum type {
	all_pieces,
	pawns,
	knights,
	bishops,
	rooks,
	queens,
	king,
	pawn_control,

	value_max
};
}


struct bitboard
{
	mutable unsigned long long b[bb_type::value_max];
};


struct position
{
	// Bit 0: can castle kingside
	// Bit 1: can castle queenside
	// Bit 2: has castled
	short castle[2];

	// 0 if en-passant not possible.
	// Otherwise board index of last-moved pawn that can be en-passanted in
	// lower 6 bits, 7th bit color of pawn that is en-passantable.
	unsigned char can_en_passant;

	// board[column][row] as piece type in lower 4 bits, color in 5th bit.
	// nil if square is empty.
	unsigned char board2[8][8];

	// Call after initializing bitboards
	void init_pawn_structure();

	struct pawn_structure {
		short eval; // From white's point of view
		unsigned long long hash;
	} pawns;

	short material[2];

	bitboard bitboards[2];
};


namespace move_flags {
enum type {
	valid = 1,
	enpassant = 2,
	promotion = 4,
	castle = 8
};
}


struct move
{
	move()
		: flags()
		, piece()
		, source_col()
		, source_row()
		, target_col()
		, target_row()
		, captured_piece()
		, promotion()
	{}

	unsigned char flags;
	pieces2::type piece;
	unsigned char source_col;
	unsigned char source_row;
	unsigned char target_col;
	unsigned char target_row;
	pieces2::type captured_piece;
	unsigned char promotion;

	bool operator!=( move const& rhs ) const {
		return source_col != rhs.source_col || source_row != rhs.source_row || target_col != rhs.target_col || target_row != rhs.target_row;
	}
	bool operator==( move const& rhs ) const {
		return source_col == rhs.source_col && source_row == rhs.source_row && target_col == rhs.target_col && target_row == rhs.target_row;
	}
};

namespace result {
enum type {
	win = 30000,
	win_threshold = win - 100,
	draw = 0,
	loss = -30000,
	loss_threshold = loss + 100
};
}

#define NULL_MOVE_REDUCTION 3

#endif
