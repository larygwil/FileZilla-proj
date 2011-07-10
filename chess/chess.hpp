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

namespace {
short piece_values[] = {
	100, 100, 100, 100, 100, 100, 100, 100,
	0, // Can't be captured
	900,
	500, 500,
	310, 310,
	300, 300
};

short promotion_values[] = {
	900,
	500,
	310,
	300
};
}

namespace special_values {
enum type
{
	knight_at_border = 25,
	castled = 50
};
}

namespace priorities
{
enum type {
	capture = 0x20,
	castle = 0x10
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

	unsigned char priority; // Left 4 bits importance, right 4 bits random shuffling
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

struct check_info {
	unsigned char check : 1;
	unsigned char multiple : 1; // If this is set, multiple pieces are checking
	unsigned char piece : 4; // Index of a piece giving check. Undefined if multiple is set
	// If check set:
	//   If multiple set: King has to move.
	//   Elif knight is set: King has to move or Knight has to be captured
	//   Else: Move king, capture piece or block line of sight
	// Else: Normal move possible

	bool operator==( check_info const& rhs ) const {
		return check == rhs.check && multiple == rhs.multiple && piece == rhs.piece;
	}
};

int const MAX_DEPTH = 8;
int const CUTOFF_DEPTH = 7;//MAX_DEPTH / 2;
int const CUTOFF_AMOUNT = 10; //shannon/3

// Features to use
//#define USE_CUTOFF

// 1: Only at depth 0
// 2: Every capture
#define USE_QUIESCENCE 1
#define USE_STATISTICS

#define USE_TRANSPOSITION 1

#endif
