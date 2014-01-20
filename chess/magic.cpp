#include "assert.hpp"
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

uint64_t* init_magic( uint64_t* offsets, uint64_t const* shifts, uint64_t const* masks, uint64_t const* multipliers, uint64_t (*attacks)(uint64_t, uint64_t, uint64_t const (&)[8][64]), uint64_t const (&rays)[8][64] ) {
	uint64_t offset = 0;
	
	for( unsigned int pi = 0; pi < 64; ++pi ) {
		offsets[pi] = offset;
		offset += 1ull << (64 - shifts[pi]);
	}

	uint64_t * values = new uint64_t[offset];
	memset( values, 0, sizeof(uint64_t) * offset );

	offset = 0;
	for( unsigned int pi = 0; pi < 64; ++pi ) {
			
		for( unsigned int i = 0; i < (1ull << (64 - shifts[pi])); ++i ) {
			uint64_t occ = expand( i, masks[pi] );

			uint64_t key = occ * multipliers[pi];
			key >>= shifts[pi];

			uint64_t const attack = attacks( pi, occ, rays );
			ASSERT( !values[offset + key] || values[offset + key] == attack );
			values[offset + key] = attack;
		}
			
		offset += 1ull << (64 - shifts[pi]);
	}

	return values;
}
}

void init_magic()
{
	uint64_t rays[8][64];
	init_rays( rays );

	rook_magic_values = init_magic( rook_magic_offsets, rook_magic_shift, rook_magic_mask, rook_magic_multiplier, rook_attacks, rays );
	bishop_magic_values = init_magic( bishop_magic_offsets, bishop_magic_shift, bishop_magic_mask, bishop_magic_multiplier, bishop_attacks, rays );
}

uint64_t rook_magic( uint64_t pi, uint64_t occ )
{
	uint64_t relevant_occ = occ & rook_magic_mask[pi];
	uint64_t key = relevant_occ * rook_magic_multiplier[pi];
	key >>= rook_magic_shift[pi];

	uint64_t offset = rook_magic_offsets[pi];

	return rook_magic_values[ offset + key ];
}

uint64_t bishop_magic( uint64_t pi, uint64_t occ )
{
	uint64_t relevant_occ = occ & bishop_magic_mask[pi];
	uint64_t key = relevant_occ * bishop_magic_multiplier[pi];
	key >>= bishop_magic_shift[pi];

	uint64_t offset = bishop_magic_offsets[pi];

	return bishop_magic_values[ offset + key ];
}
