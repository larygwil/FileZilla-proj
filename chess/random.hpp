#ifndef __RANDOM_H__
#define __RANDOM_H__

#include <random>
#include "util/mutex.hpp"

class random
{
public:
	// Auto-seeded based on time
	random();

	random( uint64_t s );

	unsigned char get_unsigned_char();
	uint64_t get_uint64();

	void seed( uint64_t seed );

private:
	mutex m_;
	std::mt19937_64 engine_;
};

#endif
