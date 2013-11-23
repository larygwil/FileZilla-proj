#include "zobrist.hpp"
#include "chess.hpp"
#include "util.hpp"
#include "random.hpp"

namespace zobrist {
	uint64_t hashes[8][2][64];

	uint64_t enpassant[64];

	uint64_t castle[2][256];

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
			zobrist::hashes[0][c][i] = 0;
			zobrist::hashes[7][c][i] = 0;
			for( int pi = pieces::pawn; pi <= pieces::king; ++pi ) {
				zobrist::hashes[pi][c][i] = rng.get_uint64();
			}
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

	zobrist::initialized = true;
}


uint64_t get_piece_hash( pieces::type pi, color::type c, uint64_t pos )
{
	return zobrist::hashes[pi][c][pos];
}
	

uint64_t get_enpassant_hash( unsigned char ep )
{
	return zobrist::enpassant[ep];
}
