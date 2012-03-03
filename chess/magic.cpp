#include "magic.hpp"
#include "magic_values.hpp"
#include "sliding_piece_attacks.hpp"

static uint64_t rook_magic_offsets[64] = {0};
static uint64_t *rook_magic_values = 0;
static uint64_t bishop_magic_offsets[64] = {0};
static uint64_t *bishop_magic_values = 0;

namespace {
// Given mask with n bits set, expands lowest n bits of value to correspond to bits of mask.
// v = 000000abc
// m = 001010010
//  -> 00a0b00c0
uint64_t expand( uint64_t value, uint64_t mask )
{
	uint64_t ret = 0;
	uint64_t bitcount = 0;

	while ( mask ) {
		uint64_t bitindex = bitscan_unset( mask );
		ret |= ((value >> bitcount) & 1ull) << bitindex;
	
		++bitcount;
	}

	return ret;
}
}


void init_magic()
{
	{
		uint64_t offset = 0;
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			rook_magic_offsets[pi] = offset;
			offset += 1ull << rook_magic_shift[pi];
		}

		rook_magic_values = new uint64_t[offset];
		memset( rook_magic_values, 0, sizeof(uint64_t) * offset );

		offset = 0;
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			
			for( unsigned int i = 0; i < (1ull << rook_magic_shift[pi]); ++i ) {
				uint64_t occ = expand( i, rook_magic_mask[pi] );

				uint64_t key = occ * rook_magic_multiplier[pi];
				key >>= (64 - rook_magic_shift[pi]);

				uint64_t const attack = rook_attacks( pi, occ );
				rook_magic_values[offset + key] = attack;
			}
			
			offset += 1ull << rook_magic_shift[pi];
		}
	}
	{
		uint64_t offset = 0;
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			bishop_magic_offsets[pi] = offset;
			offset += 1ull << bishop_magic_shift[pi];
		}

		bishop_magic_values = new uint64_t[offset];
		memset( bishop_magic_values, 0, sizeof(uint64_t) * offset );

		offset = 0;
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			
			for( unsigned int i = 0; i < (1ull << bishop_magic_shift[pi]); ++i ) {
				uint64_t occ = expand( i, bishop_magic_mask[pi] );

				uint64_t key = occ * bishop_magic_multiplier[pi];
				key >>= (64 - bishop_magic_shift[pi]);

				uint64_t const attack = bishop_attacks( pi, occ );
				bishop_magic_values[offset + key] = attack;
			}
			
			offset += 1ull << bishop_magic_shift[pi];
		}
	}
}

uint64_t rook_magic( uint64_t pi, uint64_t occ )
{
	uint64_t relevant_occ = occ & rook_magic_mask[pi];
	uint64_t key = relevant_occ * rook_magic_multiplier[pi];
	key >>= (64 - rook_magic_shift[pi]);

	uint64_t offset = rook_magic_offsets[pi];

	return rook_magic_values[ offset + key ];
}

uint64_t bishop_magic( uint64_t pi, uint64_t occ )
{
	uint64_t relevant_occ = occ & bishop_magic_mask[pi];
	uint64_t key = relevant_occ * bishop_magic_multiplier[pi];
	key >>= (64 - bishop_magic_shift[pi]);

	uint64_t offset = bishop_magic_offsets[pi];

	return bishop_magic_values[ offset + key ];
}
