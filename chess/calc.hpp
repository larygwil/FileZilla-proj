#ifndef __CALC_H__
#define __CALC_H__

#include "chess.hpp"
#include "detect_check.hpp"

bool calc( position& p, color::type c, move& m, int& res );

struct move_info {
	move m;
	short evaluation;
	unsigned char random; // A bit of random to randomly sort equally likely moves
};

struct step_data {
	short evaluation;
	char remaining_depth; // -127 on terminal position
	move_info best_move;
	short alpha;
	short beta;
};

void calculate_moves( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check );

#endif
