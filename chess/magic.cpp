#include "magic.hpp"
#include "magic_values.hpp"
#include "sliding_piece_attacks.hpp"

static unsigned long long rook_magic_offsets[64] = {0};
static unsigned long long *rook_magic_values = 0;
static unsigned long long bishop_magic_offsets[64] = {0};
static unsigned long long *bishop_magic_values = 0;

namespace {
// Given mask with n bits set, expands lowest n bits of value to correspond to bits of mask.
// v = 000000abc
// m = 001010010
//  -> 00a0b00c0
unsigned long long expand( unsigned long long value, unsigned long long mask )
{
	unsigned long long ret = 0;
	unsigned long long bitcount = 0;

	while ( mask ) {
		unsigned long long bitindex = bitscan_unset( mask );
		ret |= ((value >> bitcount) & 1ull) << bitindex;
	
		++bitcount;
	}

	return ret;
}
}


void init_magic()
{
	{
		unsigned long long offset = 0;
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			rook_magic_offsets[pi] = offset;
			offset += 1ull << rook_magic_shift[pi];
		}

		rook_magic_values = new unsigned long long[offset];
		memset( rook_magic_values, 0, sizeof(unsigned long long) * offset );

		offset = 0;
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			
			for( unsigned int i = 0; i < (1ull << rook_magic_shift[pi]); ++i ) {
				unsigned long long occ = expand( i, rook_magic_mask[pi] );

				unsigned long long key = occ * rook_magic_multiplier[pi];
				key >>= (64 - rook_magic_shift[pi]);

				unsigned long long const attack = rook_attacks( pi, occ );
				rook_magic_values[offset + key] = attack;
			}
			
			offset += 1ull << rook_magic_shift[pi];
		}
	}
	{
		unsigned long long offset = 0;
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			bishop_magic_offsets[pi] = offset;
			offset += 1ull << bishop_magic_shift[pi];
		}

		bishop_magic_values = new unsigned long long[offset];
		memset( bishop_magic_values, 0, sizeof(unsigned long long) * offset );

		offset = 0;
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			
			for( unsigned int i = 0; i < (1ull << bishop_magic_shift[pi]); ++i ) {
				unsigned long long occ = expand( i, bishop_magic_mask[pi] );

				unsigned long long key = occ * bishop_magic_multiplier[pi];
				key >>= (64 - bishop_magic_shift[pi]);

				unsigned long long const attack = bishop_attacks( pi, occ );
				bishop_magic_values[offset + key] = attack;
			}
			
			offset += 1ull << bishop_magic_shift[pi];
		}
	}
}

unsigned long long rook_magic( unsigned long long pi, unsigned long long occ )
{
	unsigned long long relevant_occ = occ & rook_magic_mask[pi];
	unsigned long long key = relevant_occ * rook_magic_multiplier[pi];
	key >>= (64 - rook_magic_shift[pi]);

	unsigned long long offset = rook_magic_offsets[pi];

	return rook_magic_values[ offset + key ];
}

unsigned long long bishop_magic( unsigned long long pi, unsigned long long occ )
{
	unsigned long long relevant_occ = occ & bishop_magic_mask[pi];
	unsigned long long key = relevant_occ * bishop_magic_multiplier[pi];
	key >>= (64 - bishop_magic_shift[pi]);

	unsigned long long offset = bishop_magic_offsets[pi];

	return bishop_magic_values[ offset + key ];
}
