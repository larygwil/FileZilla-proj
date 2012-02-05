#ifndef __CHECK_H__
#define __CHECK_H__

#include <string.h>

#include "platform.hpp"

namespace pieces {
enum type {
	none = 0,
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
	knight,
	bishop,
	rook,
	queen
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

	// Call after initializing bitboards
	void init_pawn_structure();

	void clear_bitboards();

	struct pawn_structure {
		short eval; // From white's point of view
		unsigned long long hash;
	} pawns;

	short material[2];

	bitboard bitboards[2];

	bool is_occupied_square( unsigned long long square ) const;
	unsigned long long get_occupancy( unsigned long long mask ) const;
};


namespace move_flags {
enum type {
	valid = 1,
	enpassant = 2,
	promotion = 4,
	castle = 8,
	pawn_double_move = 16
};
}


struct move
{
	move()
		: flags()
		, piece()
		, source()
		, target()
		, captured_piece()
		, promotion(promotions::queen)
	{}

	unsigned char flags;
	pieces::type piece;
	unsigned char source;
	unsigned char target;
	pieces::type captured_piece;
	unsigned char promotion;

	bool operator!=( move const& rhs ) const {
		return source != rhs.source || target != rhs.target || promotion != rhs.promotion;
	}
	bool operator==( move const& rhs ) const {
		return source == rhs.source && target == rhs.target && promotion == rhs.promotion;
	}
};

namespace result {
enum type {
	win = 31000,
	win_threshold = win - 1000,
	draw = 0,
	loss = -31000,
	loss_threshold = loss + 1000
};
}

#define NULL_MOVE_REDUCTION 3

#endif
