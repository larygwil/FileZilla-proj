#ifndef __UTIL_H__
#define __UTIL_H__

#include "chess.hpp"

#include <string>

bool validate_move( position const& p, move const& m, color::type c );
void parse_move( position& p, color::type& c, std::string const& line );

std::string move_to_string( position const& p, color::type c, move const& m );

void init_board_from_pieces( position& p );
void init_board( position& p );

bool apply_move( position& p, move const& m, color::type c );


void init_random( int seed );

// Gets pseudo-random number within the lowerst 6 bits
unsigned char get_random_6bit();

unsigned long long get_random_unsigned_long_long();

#endif
