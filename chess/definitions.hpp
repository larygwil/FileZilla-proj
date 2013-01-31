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


namespace color {
enum type {
	white = 0,
	black = 1
};
}


namespace pieces_with_color {
enum type : unsigned char {
	none = 0,
	white_pawn,
	white_knight,
	white_bishop,
	white_rook,
	white_queen,
	white_king,
	black_pawn = 9,
	black_knight,
	black_bishop,
	black_rook,
	black_queen,
	black_king
};
}

inline pieces::type get_piece( pieces_with_color::type pwc ) {
	return static_cast<pieces::type>(pwc & 0x7);
}
inline color::type get_color( pieces_with_color::type pwc ) {
	return (pwc >= pieces_with_color::black_pawn) ? color::black : color::white;
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

namespace move_flags {
enum type {
	none = 0,
	castle = 0x1000,
	enpassant = 0x2000,
	promotion = 0x3000,
	promotion_knight = 0x3000,
	promotion_bishop = 0x7000,
	promotion_rook = 0xb000,
	promotion_queen = 0xf000,
	promotion_mask = 0xc000
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
