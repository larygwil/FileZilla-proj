#ifndef __MOBILITY_H__
#define __MOBILITY_H__

#include "chess.hpp"

void evaluate_mobility( position const& p, color::type c, bitboard const* bitboards, short& mobility, short& pin );

#endif
