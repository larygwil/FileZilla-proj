#ifndef __CALC_H__
#define __CALC_H__

#include "chess.hpp"
#include "detect_check.hpp"

int const MAX_DEPTH = 8;
int const QUIESCENCE_SEARCH = 5;

struct step_data {
	short evaluation;
	move best_move;
	short alpha;
	short beta;
} __attribute__((__packed__));

bool calc( position& p, color::type c, move& m, int& res, int time_limit );
short step( int depth, int const max_depth, position const& p, unsigned long long hash, int current_evaluation, bool captured, color::type c, short alpha, short beta );

#endif
