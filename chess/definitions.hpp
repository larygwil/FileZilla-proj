#ifndef __DEFINITIONS_H__
#define __DEFINITIONS_H__

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


namespace color {
enum type {
	white = 0,
	black = 1
};
}


namespace castles {
enum type {
	none = 0,
	kingside = 1,
	queenside = 2,
	both = 3,
	has_castled = 4
};
}

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
