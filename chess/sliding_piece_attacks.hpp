#ifndef __SLIDING_PIECE_ATTACKS_H__
#define __SLIDING_PIECE_ATTACKS_H__

// This file is mainly used for boot-strapping the magic multiplication move generation.

#include "util/platform.hpp"
#include "tables.hpp"

inline uint64_t attack( uint64_t pi, uint64_t blockers, uint64_t const* const ray )
{
	uint64_t attacks = ray[pi];
	uint64_t ray_blockers = (blockers & attacks) | 0x8000000000000000ull;
	uint64_t ray_blocker = bitscan( ray_blockers );
	return attacks ^ ray[ray_blocker];
}


inline uint64_t attackr( uint64_t pi, uint64_t blockers, uint64_t const* const ray )
{
	uint64_t attacks = ray[pi];
	uint64_t ray_blockers = (blockers & attacks) | 0x1ull;
	uint64_t ray_blocker = bitscan_reverse( ray_blockers );
	return attacks ^ ray[ray_blocker];
}


inline uint64_t rook_attacks( uint64_t pi, uint64_t blockers )
{
	return attack( pi, blockers, ray_n ) |
		   attack( pi, blockers, ray_e ) |
		   attackr( pi, blockers, ray_s ) |
		   attackr( pi, blockers, ray_w );
}


inline uint64_t bishop_attacks( uint64_t pi, uint64_t blockers )
{
	return attack( pi, blockers, ray_ne ) |
		   attackr( pi, blockers, ray_se ) |
		   attackr( pi, blockers, ray_sw ) |
		   attack( pi, blockers, ray_nw );
}

#endif
