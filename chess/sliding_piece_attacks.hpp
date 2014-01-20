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

inline void init_rays( uint64_t (&rays)[8][64] )
{
		int r = 0;
		for( int dy = 1; dy >= -1; --dy ) {
			for( int dx = -1; dx <= 1; ++dx ) {
				if( !dx && !dy ) {
					continue;
				}
				for( unsigned int source = 0; source < 64; ++source ) {
					uint64_t v = 0;

					int source_col = source % 8;
					int source_row = source / 8;

					int x = source_col + dx;
					int y = source_row + dy;
					for( ; x >= 0 && x < 8 && y >= 0 && y < 8; x += dx, y += dy ) {
						v |= 1ull << (x + y * 8);
					}

					rays[r][source] = v;
				}
				++r;
			}
		}
}

inline uint64_t rook_attacks( uint64_t pi, uint64_t blockers, uint64_t const (&rays)[8][64] )
{
	return attack( pi, blockers, rays[1] ) |
		   attack( pi, blockers, rays[4] ) |
		   attackr( pi, blockers, rays[3] ) |
		   attackr( pi, blockers, rays[6] );
}

inline uint64_t bishop_attacks( uint64_t pi, uint64_t blockers, uint64_t const (&rays)[8][64] )
{
	return attack( pi, blockers, rays[0] ) |
		   attack( pi, blockers, rays[2] ) |
		   attackr( pi, blockers, rays[5] ) |
		   attackr( pi, blockers, rays[7] );
}

#endif
