#ifndef __ZOBRIST_H__
#define __ZOBRIST_H__

#include "chess.hpp"

void init_zobrist_tables();

// Get zobrist hash of position, independend of side to move
uint64_t get_zobrist_hash( position const& p );

// Same as above, but if color is black, hash is inverted.
uint64_t get_zobrist_hash( position const& p, color::type c );

uint64_t update_zobrist_hash( position const& p, uint64_t hash, move const& m );

uint64_t get_pawn_structure_hash( color::type c, unsigned char pawn );

#endif
