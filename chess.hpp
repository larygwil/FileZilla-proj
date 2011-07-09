#ifndef __CHECK_H__
#define __CHECK_H__

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

namespace special_values {
enum type
{
	knight_at_border = 25,
	castled = 25
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

struct position
{
	// [color][piece]
	piece pieces[2][16];

	// 2 bit for every pawn.
	unsigned short promotions[2];

	// board[column][row] as piece indexes in lower 4 bits, color in 5th bit.
	// nil if square is empty.
	unsigned char board[8][8];

	unsigned char can_en_passant; // Piece of last-moved player that can be en-passanted
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
	draw = 0,
	loss = -1000000,
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
	unsigned char knight : 1; // If this is set, a knight is checking. Undefined if check and multiple is set
	unsigned char piece : 4; // Index of a piece giving check. Undefined if multiple is set
	// If check set:
	//   If multiple set: King has to move.
	//   Elif knight is set: King has to move or Knight has to be captured
	//   Else: Move king, capture piece or block line of sight
	// Else: Normal move possible
};

int const MAX_DEPTH = 7;
int const CUTOFF_DEPTH = 7;//MAX_DEPTH / 2;
int const CUTOFF_AMOUNT = 10; //shannon/3

// Features to use
//#define USE_CUTOFF

// 1: Only at depth 0
// 2: Every capture
//#define USE_QUIESCENCE 1
#define USE_STATISTICS

#endif