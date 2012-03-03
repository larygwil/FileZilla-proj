#define _CRT_RAND_S
#include <stdlib.h>


#include "tables.hpp"
#include "tables.cpp"
#include "sliding_piece_attacks.hpp"
#include "platform.hpp"

#include <iomanip>
#include <iostream>

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

uint64_t rnd()
{
	unsigned int v1;
	unsigned int v2;
	rand_s(&v1);
	rand_s(&v2);

	uint64_t ret = static_cast<uint64_t>(v1) | (static_cast<uint64_t>(v2) << 32);

	return ret;
}

uint64_t xrand()
{
	uint64_t ret;
	do {
		ret = rnd() & rnd() & rnd();
	}
	while( !ret );

	return ret;
}

uint64_t gen_mask( uint64_t pi, uint64_t attack )
{
	uint64_t mask = attack;
	if( (pi % 8) != 0 ) {
		mask &= ~0x0101010101010101ull;
	}
	if( (pi % 8) != 7 ) {
		mask &= ~0x8080808080808080ull;
	}
	if( pi > 7 ) {
		mask &= ~0x00000000000000ffull;
	}
	if( pi < 56 ) {
		mask &= ~0xff00000000000000ull;
	}

	return mask;
}
}

void rooks()
{
	uint64_t rook_magic_mask[64];
	uint64_t rook_magic_shift[64];
	uint64_t rook_magic_multiplier[64];
	uint64_t rook_magic_value[64*4096];

	for( uint64_t pi = 0; pi < 64; ++pi ) {
		uint64_t const attacks = rook_attacks( pi, 0 );

		uint64_t mask = gen_mask(pi, attacks);

		rook_magic_mask[pi] = mask;

		uint64_t mask_bits = popcount( mask );
		uint64_t magic_bits = mask_bits;
		uint64_t count = 1ull << mask_bits;

		rook_magic_shift[pi] = magic_bits;

		uint64_t occ[4096];
		uint64_t acc[4096];
		for( unsigned int i = 0; i < count; ++i ) {
			occ[i] = expand( i, mask );
			acc[i] = rook_attacks( pi, occ[i] );
		}

		bool valid;
		do {
			valid = true;
			uint64_t magic_map[4096] = {0};

			uint64_t magic = xrand();
			if( popcount( ((magic * mask) & 0xFF00000000000000ull) ) < 6 ) {
				valid = false;
				continue;
			}
			for( unsigned int i = 0; i < count; ++i ) {
				uint64_t key = occ[i] * magic;

				key >>= (64 - magic_bits);
				if( !magic_map[key] ) {
					magic_map[key] = acc[i];
				}
				else if( magic_map[key] != acc[i] ) {
					valid = false;
					break;
				}
			}
			if( valid ) {
				rook_magic_multiplier[pi] = magic;
				for( unsigned int i = 0; i < count; ++i ) {
					rook_magic_value[ pi * 4096 + i] = magic_map[i];
				}
			}
		}
		while( !valid );

	}

	std::cout << "extern uint64_t const rook_magic_mask[64] = {" << std::endl;
	for( uint64_t pi = 0; pi < 64; ++pi ) {
		std::cout << "\t0x" << std::hex << std::setw(16) << std::setfill('0') << rook_magic_mask[pi] << "ull";
		if( pi != 63 ) {
			std::cout << ",";
		}
		std::cout << std::endl;
	}
	std::cout << "};" << std::endl << std::endl;

	std::cout << "extern uint64_t const rook_magic_multiplier[64] = {" << std::endl;
	for( uint64_t pi = 0; pi < 64; ++pi ) {
		std::cout << "\t0x" << std::hex << std::setw(16) << std::setfill('0') << rook_magic_multiplier[pi] << "ull";
		if( pi != 63 ) {
			std::cout << ",";
		}
		std::cout << std::endl;
	}
	std::cout << "};" << std::endl << std::endl;

	std::cout << "extern uint64_t const rook_magic_shift[64] = {" << std::endl;
	for( uint64_t pi = 0; pi < 64; ++pi ) {
		std::cout << "\t" << std::dec << std::setw(2) << std::setfill(' ') << rook_magic_shift[pi] << "ull";
		if( pi != 63 ) {
			std::cout << ",";
		}
		std::cout << std::endl;
	}
	std::cout << "};" << std::endl << std::endl;
}

void bishops()
{
	uint64_t bishop_magic_mask[64];
	uint64_t bishop_magic_shift[64];
	uint64_t bishop_magic_multiplier[64];
	uint64_t bishop_magic_value[64*4096];

	for( uint64_t pi = 0; pi < 64; ++pi ) {
		uint64_t const attacks = bishop_attacks( pi, 0 );

		uint64_t mask = gen_mask(pi, attacks);

		bishop_magic_mask[pi] = mask;

		uint64_t mask_bits = popcount( mask );
		uint64_t magic_bits = mask_bits;
		uint64_t count = 1ull << mask_bits;

		bishop_magic_shift[pi] = magic_bits;

		uint64_t occ[4096];
		uint64_t acc[4096];
		for( unsigned int i = 0; i < count; ++i ) {
			occ[i] = expand( i, mask );
			acc[i] = bishop_attacks( pi, occ[i] );
		}

		bool valid;
		do {
			valid = true;
			uint64_t magic_map[4096] = {0};

			uint64_t magic = xrand();
			if( popcount( ((magic * mask) & 0xFF00000000000000ull) ) < 6 ) {
				valid = false;
				continue;
			}
			for( unsigned int i = 0; i < count; ++i ) {
				uint64_t key = occ[i] * magic;

				key >>= (64 - magic_bits);
				if( !magic_map[key] ) {
					magic_map[key] = acc[i];
				}
				else if( magic_map[key] != acc[i] ) {
					valid = false;
					break;
				}
			}
			if( valid ) {
				bishop_magic_multiplier[pi] = magic;
				for( unsigned int i = 0; i < count; ++i ) {
					bishop_magic_value[ pi * 4096 + i] = magic_map[i];
				}
			}
		}
		while( !valid );

	}

	std::cout << "extern uint64_t const bishop_magic_mask[64] = {" << std::endl;
	for( uint64_t pi = 0; pi < 64; ++pi ) {
		std::cout << "\t0x" << std::hex << std::setw(16) << std::setfill('0') << bishop_magic_mask[pi] << "ull";
		if( pi != 63 ) {
			std::cout << ",";
		}
		std::cout << std::endl;
	}
	std::cout << "};" << std::endl << std::endl;

	std::cout << "extern uint64_t const bishop_magic_multiplier[64] = {" << std::endl;
	for( uint64_t pi = 0; pi < 64; ++pi ) {
		std::cout << "\t0x" << std::hex << std::setw(16) << std::setfill('0') << bishop_magic_multiplier[pi] << "ull";
		if( pi != 63 ) {
			std::cout << ",";
		}
		std::cout << std::endl;
	}
	std::cout << "};" << std::endl << std::endl;

	std::cout << "extern uint64_t const bishop_magic_shift[64] = {" << std::endl;
	for( uint64_t pi = 0; pi < 64; ++pi ) {
		std::cout << "\t" << std::dec << std::setw(2) << std::setfill(' ') << bishop_magic_shift[pi] << "ull";
		if( pi != 63 ) {
			std::cout << ",";
		}
		std::cout << std::endl;
	}
	std::cout << "};" << std::endl << std::endl;
}

int main()
{
//	rooks();
	bishops();
}
