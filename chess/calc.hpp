#ifndef __CALC_H__
#define __CALC_H__

#include "chess.hpp"
#include "detect_check.hpp"
#include "pvlist.hpp"
#include "moves.hpp"


class seen_positions {
public:
	seen_positions();

	unsigned long long pos[200]; // Must be at least 50 full moves + max depth
	int root_position; // Index of first move after root position in seen_positions
	int null_move_position;

	bool is_two_fold( unsigned long long hash, int depth ) const;
	bool is_three_fold( unsigned long long hash, int depth ) const;
};

// May modify seen_positions at indexes > root_position
bool calc( position& p, color::type c, move& m, int& res, unsigned long long time_limit, int clock, seen_positions& seen );

class context
{
public:
	context()
		: max_depth(1)
		, quiescence_depth(0)
		, clock(0)
		, move_ptr(moves)
	{
	}

	int max_depth;
	int quiescence_depth;
	unsigned char clock; // The halfmove clock
	pv_entry_pool pv_pool;

	move_info moves[200 * 25];
	move_info* move_ptr;

	seen_positions seen;
};

short step( int depth, context& ctx, position const& p, unsigned long long hash, int current_evaluation, color::type c, short alpha, short beta, pv_entry* pv, bool last_was_null );

#endif
