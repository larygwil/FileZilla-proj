#ifndef __MOBILITY_H__
#define __MOBILITY_H__

#include "chess.hpp"

short get_mobility( position const& p, bitboard const* bitboards );

short get_pins( position const& p, bitboard const* bitboards );

#endif
