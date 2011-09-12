#ifndef __CALC_H__
#define __CALC_H__

#include "config.hpp"
#include "chess.hpp"
#include "detect_check.hpp"
#include "pvlist.hpp"
#include "moves.hpp"


class seen_positions {
public:
	seen_positions();

	unsigned long long pos[100 + MAX_DEPTH + MAX_QDEPTH + 10]; // Must be at least 50 full moves + max depth and add some safety margin.
	int root_position; // Index of first move after root position in seen_positions
	int null_move_position;

	bool is_two_fold( unsigned long long hash, int depth ) const;
	bool is_three_fold( unsigned long long hash, int depth ) const;
};

struct new_best_move_callback
{
	virtual void on_new_best_move( position const& p, color::type c, int depth, int evaluation, unsigned long long nodes, pv_entry const* pv );
};
extern new_best_move_callback default_new_best_move_callback;

// May modify seen_positions at indexes > root_position
bool calc( position& p, color::type c, move& m, int& res, unsigned long long move_time_limit, unsigned long long time_remaining, int clock, seen_positions& seen, new_best_move_callback& new_best_cb = default_new_best_move_callback );

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

	move_info moves[200 * (MAX_DEPTH + MAX_QDEPTH)];
	move_info* move_ptr;

	seen_positions seen;
};

short step( int depth, context& ctx, position const& p, unsigned long long hash, int current_evaluation, color::type c, short alpha, short beta, pv_entry* pv, bool last_was_null );

#endif
