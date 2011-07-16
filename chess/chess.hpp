#ifndef __CHECK_H__
#define __CHECK_H__

#include <string.h>

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

struct position_base
{
	// [color][piece]
	piece pieces[2][16];

	// 2 bit for every pawn.
	unsigned short promotions[2];

	unsigned char can_en_passant; // Piece of last-moved player that can be en-passanted

	bool operator==( position_base const& rhs ) const {
		return !memcmp(pieces, rhs.pieces, sizeof(pieces) ) &&
				promotions[0] == rhs.promotions[0] &&
				promotions[1] == rhs.promotions[1] &&
				can_en_passant == rhs.can_en_passant;
	}
	bool operator<( position_base const& rhs ) const {
		int cmp = memcmp(&pieces, &rhs.pieces, 32 * sizeof(piece) );
		if( cmp ) {
			return cmp < 0;
		}
		if( promotions[0] < rhs.promotions[0] ) {
			return true;
		}
		if( promotions[0] > rhs.promotions[0] ) {
			return false;
		}
		if( promotions[1] < rhs.promotions[1] ) {
			return true;
		}
		if( promotions[1] > rhs.promotions[1] ) {
			return false;
		}
		return can_en_passant < rhs.can_en_passant;
	}
};


struct position : public position_base
{
	// board[column][row] as piece indexes in lower 4 bits, color in 5th bit.
	// nil if square is empty.
	unsigned char board[8][8];
};


struct move
{
	unsigned char piece : 4;
	unsigned char target_col : 3;
	unsigned char target_row : 3;

	bool operator!=( move const& rhs ) const {
		return piece != rhs.piece || target_col != rhs.target_col || target_row != rhs.target_row;
	}
};


namespace result {
enum type {
	win = 1000000,
	win_threshold = win - 100,
	draw = 0,
	loss = -1000000,
	loss_threshold = loss + 100
};
}

namespace color {
enum type {
	white = 0,
	black = 1
};
}

#define USE_QUIESCENCE 1

#define USE_TRANSPOSITION 1

#endif
