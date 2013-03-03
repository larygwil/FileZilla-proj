#ifndef __ZOBRIST_H__
#define __ZOBRIST_H__

#include "chess.hpp"

void init_zobrist_tables();

uint64_t get_pawn_structure_hash( color::type c, unsigned char pawn );

uint64_t get_enpassant_hash( unsigned char ep );

uint64_t get_piece_hash( pieces::type pi, color::type c, int pos );

namespace zobrist {
	extern uint64_t pawns[2][64];
	extern uint64_t knights[2][64];
	extern uint64_t bishops[2][64];
	extern uint64_t rooks[2][64];
	extern uint64_t queens[2][64];
	extern uint64_t kings[2][64];

	extern uint64_t enpassant[64];

	extern uint64_t castle[2][5];

	extern uint64_t pawn_structure[2][64];

	extern bool initialized;
}

#endif
