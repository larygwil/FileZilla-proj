#ifndef __RANDOM_H__
#define __RANDOM_H__

#include <random>
#include "util/mutex.hpp"

class randgen
{
public:
	// Auto-seeded based on time
	randgen();

	randgen( uint64_t s );

	unsigned char get_unsigned_char();
	uint64_t get_uint64();

	uint64_t seed( uint64_t s = get_time() );

private:
	mutex m_;
	std::mt19937_64 engine_;
};

#endif
