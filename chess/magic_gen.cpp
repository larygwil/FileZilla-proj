#define _CRT_RAND_S
#include <stdlib.h>

#include "tables.hpp"
#include "tables.cpp"
#include "sliding_piece_attacks.hpp"
#include "util/platform.hpp"

#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <time.h>

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

std::mt19937 engine;

uint64_t rnd()
{
	uint64_t ret = (static_cast<uint64_t>(engine()) << 32) | engine();
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

void calc_magic( std::string const& name, uint64_t (*attack_gen)(uint64_t, uint64_t) )
{
	std::cerr << "Generating " << name << " magics..." << std::endl;

	uint64_t magic_mask[64];
	uint64_t magic_shift[64];
	uint64_t magic_multiplier[64];

	for( uint64_t pi = 0; pi < 64; ++pi ) {
		uint64_t const attacks = attack_gen( pi, 0 );
		uint64_t mask = gen_mask(pi, attacks);
		magic_mask[pi] = mask;
		uint64_t mask_bits = popcount( mask );
		magic_shift[pi] = mask_bits;
		uint64_t const count = 1ull << mask_bits;

		uint64_t occ[4096] = {0};
		uint64_t acc[4096] = {0};
		for( unsigned int i = 0; i < count; ++i ) {
			occ[i] = expand( i, mask );
			acc[i] = attack_gen( pi, occ[i] );
		}

		uint64_t magic_shift = mask_bits;
		uint64_t magic;

		bool valid = false;
		while( !valid ) {
			uint64_t magic_map[4096] = {0};

			magic = xrand();

			valid = true;
			for( unsigned int i = 0; i < count; ++i ) {
				uint64_t key = occ[i] * magic;

				key >>= (64 - magic_shift);
				if( !magic_map[key] ) {
					magic_map[key] = acc[i];
				}
				else if( magic_map[key] != acc[i] ) {
					valid = false;
					break;
				}
			}
		}

		magic_multiplier[pi] = magic;
	}

	std::cerr << "Generating and printing " << name << " tables" << std::endl;

	std::cout << "extern uint64_t const " << name << "_magic_mask[64] = {" << std::endl;
	for( uint64_t pi = 0; pi < 64; ++pi ) {
		std::cout << "\t0x" << std::hex << std::setw(16) << std::setfill('0') << magic_mask[pi] << "ull";
		if( pi != 63 ) {
			std::cout << ",";
		}
		std::cout << std::endl;
	}
	std::cout << "};" << std::endl << std::endl;

	std::cout << "extern uint64_t const " << name << "_magic_multiplier[64] = {" << std::endl;
	for( uint64_t pi = 0; pi < 64; ++pi ) {
		std::cout << "\t0x" << std::hex << std::setw(16) << std::setfill('0') << magic_multiplier[pi] << "ull";
		if( pi != 63 ) {
			std::cout << ",";
		}
		std::cout << std::endl;
	}
	std::cout << "};" << std::endl << std::endl;

	std::cout << "extern uint64_t const " << name << "_magic_shift[64] = {" << std::endl;
	for( uint64_t pi = 0; pi < 64; ++pi ) {
		std::cout << "\t" << std::dec << std::setw(2) << std::setfill(' ') << (64-magic_shift[pi]) << "ull";
		if( pi != 63 ) {
			std::cout << ",";
		}
		std::cout << std::endl;
	}
	std::cout << "};" << std::endl << std::endl;
}

int main()
{
	engine.seed(static_cast<unsigned long>(time(0)));

	calc_magic( "rook", rook_attacks );
	calc_magic( "bishop", bishop_attacks );
}
