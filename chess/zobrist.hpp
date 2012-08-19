#ifndef __ZOBRIST_H__
#define __ZOBRIST_H__

#include "chess.hpp"

void init_zobrist_tables();

// Get zobrist hash of position
uint64_t get_zobrist_hash( position const& p );

uint64_t update_zobrist_hash( position const& p, uint64_t hash, move const& m );

uint64_t get_pawn_structure_hash( color::type c, unsigned char pawn );

uint64_t get_enpassant_hash( unsigned char ep );

#endif
