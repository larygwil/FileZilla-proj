#ifndef __MAGIC_H__
#define __MAGIC_H__

#include "chess.hpp"

void init_magic();

unsigned long long rook_magic( unsigned long long pi, unsigned long long occ );
unsigned long long bishop_magic( unsigned long long pi, unsigned long long occ );

#endif