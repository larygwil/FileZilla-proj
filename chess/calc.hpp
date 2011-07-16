#ifndef __CALC_H__
#define __CALC_H__

#include "chess.hpp"
#include "detect_check.hpp"

bool calc( position& p, color::type c, move& m, int& res );

struct move_info {
	move m;
	position new_pos;
	int evaluation;
	bool captured;
};

struct step_data {
	int evaluation;
	bool terminal;
	check_map check;
	move_info best_move;
	int remaining_depth;
	int alpha;
	int beta;
};

void calculate_moves( position const& p, color::type c, move_info*& moves, check_map const& check );

#endif
