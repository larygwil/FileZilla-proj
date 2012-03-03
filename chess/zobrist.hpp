#ifndef __ZOBRIST_H__
#define __ZOBRIST_H__

#include "chess.hpp"

void init_zobrist_tables();

uint64_t get_zobrist_hash( position const& p );

uint64_t update_zobrist_hash( position const& p, color::type c, uint64_t hash, move const& m );

uint64_t get_pawn_structure_hash( color::type c, unsigned char pawn );

#endif
