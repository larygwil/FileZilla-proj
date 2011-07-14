#ifndef __CALC_H__
#define __CALC_H__

#include "chess.hpp"
#include "detect_check.hpp"

#include <vector>

int evaluate( color::type c, position const& p );

bool calc( position& p, color::type c, move& m, int& res );

struct move_info {
	move m;
	position new_pos;
	int evaluation;
	bool captured;
};
typedef std::vector<move_info> possible_moves;

void calculate_moves( position const& p, color::type c, possible_moves& moves, check_map const& check );

#endif
