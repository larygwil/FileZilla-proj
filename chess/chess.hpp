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
	mutable uint64_t b[bb_type::value_max];
};


namespace castles {
enum type {
	none = 0,
	kingside = 1,
	queenside = 2,
	both = 3,
	has_castled = 4
};
}

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
		uint64_t hash;
		uint64_t passed;
	} pawns;

	short material[2];

	bitboard bitboards[2];

	bool is_occupied_square( uint64_t square ) const;
	uint64_t get_occupancy( uint64_t mask ) const;
};


namespace move_flags {
enum type {
	none = 0,
	enpassant = 1,
	castle = 2,
	pawn_double_move = 4,
	promotion_knight = 16,
	promotion_bishop = 24,
	promotion_rook = 32,
	promotion_queen = 40,
	promotion_mask = 56,
	promotion_shift = 3
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
	{}

	bool empty() const { return piece == pieces::none; }

	unsigned char flags;
	pieces::type piece;
	unsigned char source;
	unsigned char target;
	pieces::type captured_piece;

	bool operator!=( move const& rhs ) const {
		return flags != rhs.flags || piece != rhs.piece || source != rhs.source || target != rhs.target || captured_piece != rhs.captured_piece;
	}
	bool operator==( move const& rhs ) const {
		return flags == rhs.flags && piece == rhs.piece && source == rhs.source && target == rhs.target && captured_piece == rhs.captured_piece;
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
