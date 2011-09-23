#ifndef __SLIDING_PIECE_ATTACKS_H__
#define __SLIDING_PIECE_ATTACKS_H__

#include "platform.hpp"

extern unsigned long long visibility_bishop[64];
extern unsigned long long visibility_rook[64];
extern unsigned long long ray_n[64];
extern unsigned long long ray_e[64];
extern unsigned long long ray_s[64];
extern unsigned long long ray_w[64];
extern unsigned long long ray_ne[64];
extern unsigned long long ray_se[64];
extern unsigned long long ray_sw[64];
extern unsigned long long ray_nw[64];

inline unsigned long long attack( unsigned long long pi, unsigned long long blockers, unsigned long long const* const ray )
{
	unsigned long long attacks = ray[pi];
	unsigned long long ray_blockers = (blockers & attacks) | 0x8000000000000000ull;
	unsigned long long ray_blocker;
	bitscan( ray_blockers, ray_blocker );
	return attacks ^ ray[ray_blocker];
}


inline unsigned long long attackr( unsigned long long pi, unsigned long long blockers, unsigned long long const* const ray )
{
	unsigned long long attacks = ray[pi];
	unsigned long long ray_blockers = (blockers & attacks) | 0x1ull;
	unsigned long long ray_blocker;
	bitscan_reverse( ray_blockers, ray_blocker);
	return attacks ^ ray[ray_blocker];
}


inline unsigned long long rook_attacks( unsigned long long pi, unsigned long long blockers )
{
	return attack( pi, blockers, ray_n ) |
		   attack( pi, blockers, ray_e ) |
		   attackr( pi, blockers, ray_s ) |
		   attackr( pi, blockers, ray_w );
}


inline unsigned long long bishop_attacks( unsigned long long pi, unsigned long long blockers )
{
	return attack( pi, blockers, ray_ne ) |
		   attackr( pi, blockers, ray_se ) |
		   attackr( pi, blockers, ray_sw ) |
		   attack( pi, blockers, ray_nw );
}

#endif
