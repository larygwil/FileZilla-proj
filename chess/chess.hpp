#ifndef __CHECK_H__
#define __CHECK_H__

#include <string.h>

#include "platform.hpp"
#include "score.hpp"

namespace pieces {
enum type : unsigned char {
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

class position
{
public:
	// Bit 0: can castle kingside
	// Bit 1: can castle queenside
	// Bit 2: has castled
	short castle[2];

	// 0 if en-passant not possible.
	// Otherwise board index of last-moved pawn that can be en-passanted in
	// lower 6 bits, 7th bit color of pawn that is en-passantable.
	unsigned char can_en_passant;

	// After setting bitboards, castling rights and en-passant square,
	// call this function to 
	void update_derived();

	void clear_bitboards();

	uint64_t pawn_hash;

	score material[2];

	// Material and pst, nothing else.
	score base_eval;

	bitboard bitboards[2];

	int king_pos[2];

	bool is_occupied_square( uint64_t square ) const;
	uint64_t get_occupancy( uint64_t mask ) const;

	bool verify() const;

private:
	void init_pawn_hash();
	void init_material();
	void init_eval();
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


PACKED(struct move,
{
	move()
		: u()
	{}

	bool empty() const { return piece == pieces::none; }

	union {
		uint64_t u;
		PACKED(struct,
		{
			unsigned char flags;
			pieces::type piece;
			unsigned char source;
			unsigned char target;
			pieces::type captured_piece;
		});
	};

	bool operator!=( move const& rhs ) const {
		return u != rhs.u;
	}
	bool operator==( move const& rhs ) const {
		return u == rhs.u;
	}
});

namespace result {
enum type {
	win = 31000,
	win_threshold = win - 1000,
	draw = 0,
	loss = -31000,
	loss_threshold = loss + 1000
};
}

#endif
