#include "zobrist.hpp"
#include "chess.hpp"
#include "util.hpp"
#include "random.hpp"

namespace zobrist {
	uint64_t pawns[2][64];
	uint64_t knights[2][64];
	uint64_t bishops[2][64];
	uint64_t rooks[2][64];
	uint64_t queens[2][64];
	uint64_t kings[2][64];

	uint64_t enpassant[64];

	uint64_t castle[2][4];

	uint64_t pawn_structure[2][64];

	bool initialized = false;
}

extern unsigned char const queenside_rook_origin[2] = {
	0, 56
};
extern unsigned char const kingside_rook_origin[2] = {
	7, 63
};


void init_zobrist_tables()
{
	if( zobrist::initialized ) {
		return;
	}

	push_rng_state();
	init_random( 42 );

	for( int c = 0; c < 2; ++c ) {
		for( int i = 0; i < 64; ++i ) {
			zobrist::pawns[c][i] = get_random_unsigned_long_long();
			zobrist::knights[c][i] = get_random_unsigned_long_long();
			zobrist::bishops[c][i] = get_random_unsigned_long_long();
			zobrist::rooks[c][i] = get_random_unsigned_long_long();
			zobrist::queens[c][i] = get_random_unsigned_long_long();
			zobrist::kings[c][i] = get_random_unsigned_long_long();
		}
	}
	
	for( unsigned int c = 0; c < 2; ++c ) {
		zobrist::castle[c][0] = 0;
		zobrist::castle[c][1] = get_random_unsigned_long_long();
		zobrist::castle[c][2] = get_random_unsigned_long_long();
		zobrist::castle[c][3] = zobrist::castle[c][1] ^ zobrist::castle[c][2];
	}

	zobrist::enpassant[0] = 0;
	for( unsigned int i = 1; i < 64; ++i ) {
		if( i / 8 == 2 || i / 8 == 5 ) {
			zobrist::enpassant[i] = get_random_unsigned_long_long();
		}
		else {
			zobrist::enpassant[i] = 0;
		}
	}

	for( unsigned int c = 0; c < 2; ++c ) {
		for( unsigned int pawn = 0; pawn < 64; ++pawn ) {
			zobrist::pawn_structure[c][pawn] = get_random_unsigned_long_long();
		}
	}

	zobrist::initialized = true;

	pop_rng_state();
}


uint64_t get_piece_hash( pieces::type pi, color::type c, int pos )
{
	switch( pi ) {
		case pieces::pawn:
			return zobrist::pawns[c][pos];
		case pieces::knight:
			return zobrist::knights[c][pos];
		case pieces::bishop:
			return zobrist::bishops[c][pos];
		case pieces::rook:
			return zobrist::rooks[c][pos];
		case pieces::queen:
			return zobrist::queens[c][pos];
		case pieces::king:
			return zobrist::kings[c][pos];
		default:
			return 0;
	}
}
	
uint64_t get_pawn_structure_hash( color::type c, unsigned char pawn )
{
	return zobrist::pawn_structure[c][pawn];
}


uint64_t get_enpassant_hash( unsigned char ep )
{
	return zobrist::enpassant[ep];
}
