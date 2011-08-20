#ifndef __CALC_H__
#define __CALC_H__

#include "chess.hpp"
#include "detect_check.hpp"

bool calc( position& p, color::type c, move& m, int& res, int time_limit, int clock );

struct context
{
	int max_depth;
	int quiescence_depth;
	unsigned char clock; // The halfmove clock
};

short step( int depth, context const& ctx, position const& p, unsigned long long hash, int current_evaluation, color::type c, short alpha, short beta );

#endif
