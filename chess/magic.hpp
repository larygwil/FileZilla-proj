#ifndef __MAGIC_H__
#define __MAGIC_H__

#include "chess.hpp"

void init_magic();

uint64_t rook_magic( uint64_t pi, uint64_t occ );
uint64_t bishop_magic( uint64_t pi, uint64_t occ );

#endif