#ifndef __MOVES_H__
#define __MOVES_H__

#include "chess.hpp"
#include "detect_check.hpp"

struct move_info {
	move m;
	short evaluation;
	unsigned char random; // A bit of random to randomly sort equally likely moves
};

void calculate_moves( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, bool const captures_only = false );

#endif
