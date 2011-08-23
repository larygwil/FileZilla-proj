#ifndef __CALC_H__
#define __CALC_H__

#include "chess.hpp"
#include "detect_check.hpp"
#include "pvlist.hpp"

bool calc( position& p, color::type c, move& m, int& res, int time_limit, int clock );

struct context
{
	int max_depth;
	int quiescence_depth;
	unsigned char clock; // The halfmove clock
	pv_entry_pool pv_pool;
};

short step( int depth, context& ctx, position const& p, unsigned long long hash, int current_evaluation, color::type c, short alpha, short beta, pv_entry* pv );

#endif
