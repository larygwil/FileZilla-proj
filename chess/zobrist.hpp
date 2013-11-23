#ifndef __ZOBRIST_H__
#define __ZOBRIST_H__

#include "chess.hpp"

void init_zobrist_tables();

uint64_t get_enpassant_hash( unsigned char ep );

uint64_t get_piece_hash( pieces::type pi, color::type c, uint64_t pos );

namespace zobrist {
	extern uint64_t hashes[8][2][64];
	
	extern uint64_t enpassant[64];

	extern uint64_t castle[2][256];

	extern bool initialized;
}

#endif
