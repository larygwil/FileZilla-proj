#ifndef __CHECK_H__
#define __CHECK_H__

#include <string.h>

#include "platform.hpp"

namespace pieces {
enum type {
	pawn1,
	pawn2,
	pawn3,
	pawn4,
	pawn5,
	pawn6,
	pawn7,
	pawn8,
	king,
	queen,
	rook1,
	rook2,
	bishop1,
	bishop2,
	knight1,
	knight2,
	nil = 255
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

struct piece
{
	unsigned char alive : 1;
	unsigned char column : 3;
	unsigned char row : 3;

	// For knights, indicates whether they can castle.
	// For king, indicates whether it has castled
	// For pawns:
	//   Alive set: promoted
	unsigned char special : 1;
};


namespace color {
enum type {
	white = 0,
	black = 1
};
}


PACKED(struct position
{
	// [color][piece]
	piece pieces[2][16];

	// 2 bit for every pawn.
	unsigned short promotions[2];

	// pieces::nil if en-passant not possible.
	// Otherwise piece of last-moved player that can be en-passanted in
	// lower 4 bits, 5th bit color of pawn that is en-passantable.
	unsigned char can_en_passant;

	// board[column][row] as piece indexes in lower 4 bits, color in 5th bit.
	// nil if square is empty.
	unsigned char board[8][8];

	void calc_pawn_map();

	void evaluate_pawn_structure();

	struct pawn_structure {
		unsigned long long map[2];
		unsigned short eval; // From white's point of view
		unsigned long long hash;
	} pawns;

	short material[2];
	short tropism[2];

});


struct bitboard
{
	unsigned long long all_pieces;
	unsigned long long pawns;
	unsigned long long knights;
	unsigned long long bishops;
	unsigned long long rooks;
	unsigned long long queens;
	unsigned long long king;

	unsigned long long pawn_control;
};


struct move
{
	move()
		: source_col()
		, source_row()
		, target_col()
		, target_row()
		, other()
	{}

	unsigned char source_col;
	unsigned char source_row;
	unsigned char target_col;
	unsigned char target_row;
	unsigned char other;

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


#define USE_QUIESCENCE 1

#define NULL_MOVE_REDUCTION 2

#endif
