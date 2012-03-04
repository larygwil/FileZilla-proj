#ifndef __RANDOM_H__
#define __RANDOM_H__

void init_random( uint64_t seed );

void push_rng_state();
void pop_rng_state();

unsigned char get_random_unsigned_char();

uint64_t get_random_unsigned_long_long();

#endif
