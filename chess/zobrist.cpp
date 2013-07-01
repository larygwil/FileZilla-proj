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

	uint64_t castle[2][256];

	uint64_t pawn_structure[2][64];

	bool initialized = false;
}

void init_zobrist_tables()
{
	if( zobrist::initialized ) {
		return;
	}

	randgen rng( 42 );

	for( int c = 0; c < 2; ++c ) {
		for( int i = 0; i < 64; ++i ) {
			zobrist::pawns[c][i] = rng.get_uint64();
			zobrist::knights[c][i] = rng.get_uint64();
			zobrist::bishops[c][i] = rng.get_uint64();
			zobrist::rooks[c][i] = rng.get_uint64();
			zobrist::queens[c][i] = rng.get_uint64();
			zobrist::kings[c][i] = rng.get_uint64();
		}
	}

	for( unsigned int c = 0; c < 2; ++c ) {
		for( unsigned int i = 0; i < 8; ++i ) {
			zobrist::castle[c][1ull << i] = rng.get_uint64();
		}
		for( unsigned int i = 0; i < 8; ++i ) {
			for( unsigned int j = i + 1; j < 8; ++j ) {
				zobrist::castle[c][(1ull << i) | (1ull << j)] = zobrist::castle[c][1ull << i] ^ zobrist::castle[c][1ull << j];
			}

		}
	}

	zobrist::enpassant[0] = 0;
	for( unsigned int i = 1; i < 64; ++i ) {
		if( i / 8 == 2 || i / 8 == 5 ) {
			zobrist::enpassant[i] = rng.get_uint64();
		}
		else {
			zobrist::enpassant[i] = 0;
		}
	}

	for( unsigned int c = 0; c < 2; ++c ) {
		for( unsigned int pawn = 0; pawn < 64; ++pawn ) {
			zobrist::pawn_structure[c][pawn] = rng.get_uint64();
		}
	}

	zobrist::initialized = true;
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
