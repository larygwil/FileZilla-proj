#include "util/platform.hpp"
#include "util/mutex.hpp"
#include "random.hpp"

#include <list>
#include <iostream>
#include <random>
#include <time.h>

namespace {
static mutex m;
static std::list<std::mt19937_64> stack;
std::mt19937_64 engine;
}

void init_random( uint64_t seed )
{
	engine.seed( static_cast<unsigned long>(seed) );
}


unsigned char get_random_unsigned_char()
{
	scoped_lock l( m ) ;
	return static_cast<unsigned char>(engine());
}
		
uint64_t get_random_unsigned_long_long()
{
	scoped_lock l( m ) ;
	uint64_t ret = engine();
	return ret;
}

void push_rng_state()
{
	scoped_lock l( m ) ;
	stack.push_back( engine );
}

void pop_rng_state()
{
	scoped_lock l( m );
	if( stack.empty() ) {
		std::cerr << "Cannot pop rng state if empty" << std::endl;
		abort();
	}

	engine = stack.back();
	stack.pop_back();
}
