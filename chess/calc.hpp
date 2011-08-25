#ifndef __CALC_H__
#define __CALC_H__

#include "chess.hpp"
#include "detect_check.hpp"
#include "pvlist.hpp"


class seen_positions {
public:
	unsigned long long pos[150]; // Must be at least 50 full moves + max depth
	int root_position; // Index of first move after root position in seen_positions

	bool is_two_fold( unsigned long long hash, int depth ) const;
	bool is_three_fold( unsigned long long hash, int depth ) const;
};

// May modify seen_positions at indexes > root_position
bool calc( position& p, color::type c, move& m, int& res, int time_limit, int clock, seen_positions& seen );

struct context
{
	int max_depth;
	int quiescence_depth;
	unsigned char clock; // The halfmove clock
	pv_entry_pool pv_pool;

	seen_positions seen;
};

short step( int depth, context& ctx, position const& p, unsigned long long hash, int current_evaluation, color::type c, short alpha, short beta, pv_entry* pv );

#endif
