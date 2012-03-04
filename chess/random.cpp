#include "platform.hpp"
#include "random.hpp"
#include "random_data.hpp"

#include <list>
#include <iostream>

namespace {
static mutex m;
static uint64_t random_unsigned_long_long_pos = 0;
static uint64_t random_unsigned_char = 0;
static std::list<uint64_t> stack;
}

void init_random( uint64_t seed )
{
	random_unsigned_char = seed;
	random_unsigned_long_long_pos = (seed + 0xf00) % sizeof(precomputed_random_data);
}


unsigned char get_random_unsigned_char()
{
	scoped_lock l( m ) ;
	if( ++random_unsigned_char >= sizeof(precomputed_random_data) ) {
		random_unsigned_char = 0;
	}
	return precomputed_random_data[random_unsigned_char];
}

uint64_t get_random_unsigned_long_long()
{
	scoped_lock l( m );
	random_unsigned_long_long_pos += sizeof( uint64_t );

	if( random_unsigned_long_long_pos >= (sizeof(precomputed_random_data) - 8) ) {
		random_unsigned_long_long_pos = 0;
	}

	uint64_t ret = *reinterpret_cast<uint64_t*>(precomputed_random_data + random_unsigned_long_long_pos);
	return ret;
}

void push_rng_state()
{
	scoped_lock l( m ) ;
	stack.push_back( random_unsigned_char );
	stack.push_back( random_unsigned_long_long_pos );
}

void pop_rng_state()
{
	scoped_lock l( m );
	if( stack.size() < 2 ) {
		std::cerr << "Cannot pop rng state if empty" << std::endl;
		exit(1);
	}

	random_unsigned_long_long_pos = stack.back();
	stack.pop_back();
	random_unsigned_char = stack.back();
	stack.pop_back();
}
