#ifndef __ZOBRIST_H__
#define __ZOBRIST_H__

#include "chess.hpp"

void init_zobrist_tables();

unsigned long long get_zobrist_hash( position const& p, color::type c );

unsigned long long update_zobrist_hash( position const& p, color::type c, unsigned long long hash, move const& m );

unsigned long long get_pawn_structure_hash( color::type c, unsigned char pawn );

#endif
