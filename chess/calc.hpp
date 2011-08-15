#ifndef __CALC_H__
#define __CALC_H__

#include "chess.hpp"
#include "detect_check.hpp"

struct step_data {
	short evaluation;
	move best_move;
	short alpha;
	short beta;
	signed char remaining_depth;
} __attribute__((__packed__));

bool calc( position& p, color::type c, move& m, int& res, int time_limit );

struct context
{
	int max_depth;
	int quiescence_depth;
};

short step( int depth, context const& ctx, position const& p, unsigned long long hash, int current_evaluation, color::type c, short alpha, short beta );

#endif
