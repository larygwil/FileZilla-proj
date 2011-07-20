#ifndef __CALC_H__
#define __CALC_H__

#include "chess.hpp"
#include "detect_check.hpp"

struct step_data {
	short evaluation;
	char remaining_depth; // -127 on terminal position
	move best_move;
	short alpha;
	short beta;
};

bool calc( position& p, color::type c, move& m, int& res );

#endif
