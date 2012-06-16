#ifndef __CALC_H__
#define __CALC_H__

#include "config.hpp"
#include "chess.hpp"
#include "detect_check.hpp"
#include "history.hpp"
#include "pvlist.hpp"
#include "moves.hpp"
#include "seen_positions.hpp"

int const depth_factor = 6;

struct new_best_move_callback_base
{
	virtual void on_new_best_move( position const& p, color::type c, int depth, int selective_depth, int evaluation, uint64_t nodes, duration const& elapsed, pv_entry const* pv ) = 0;
};

struct def_new_best_move_callback : public new_best_move_callback_base
{
	virtual void on_new_best_move( position const& p, color::type c, int depth, int selective_depth, int evaluation, uint64_t nodes, duration const& elapsed, pv_entry const* pv );
};

struct null_new_best_move_callback : public new_best_move_callback_base
{
	virtual void on_new_best_move( position const&, color::type, int, int, int, uint64_t, duration const&, pv_entry const* ) {}
};

extern def_new_best_move_callback default_new_best_move_callback;
extern null_new_best_move_callback null_new_best_move_cb;

class calc_result
{
public:
	calc_result()
		: forecast()
	{
	}

	short forecast;
	move best_move;

	// Extra time spent, e.g. due to changed PV
	duration used_extra_time;
};

class calc_manager
{
public:
	calc_manager();
	virtual ~calc_manager();

	// May modify seen_positions at indexes > root_position
	// move_time_limit is the desired time we should calculate.
	// deadline is the maximum time we may calculate without losing the game.
	calc_result calc( position& p, color::type c,
		   duration const& move_time_limit, duration const& deadline, int clock,
		   seen_positions& seen, short last_mate,
		   new_best_move_callback_base& new_best_cb = default_new_best_move_callback );

private:
	class impl;
	impl* impl_;
};

class killer_moves
{
public:
	killer_moves() {}

	void add_killer( move const& m ) {
		if( m1 != m ) {
			m2 = m1;
			m1 = m;
		}
	}

	bool is_killer( move const& m ) const {
		return m == m1 || m == m2;
	}

	move m1;
	move m2;
};

class context
{
public:
	context()
		: clock(0)
		, move_ptr(moves)
	{
	}

	unsigned char clock; // The halfmove clock
	pv_entry_pool pv_pool;

	move_info moves[200 * (MAX_DEPTH + MAX_QDEPTH)];
	move_info* move_ptr;

	seen_positions seen;

	killer_moves killers[2][MAX_DEPTH + 1];

	history history_;
};

// Depth is number of plies to search multiplied by depth_factor
short step( int depth, int ply, context& ctx, position const& p, uint64_t hash, color::type c, check_map const& check, short alpha, short beta, pv_entry* pv, bool last_was_null, short full_eval = result::win, unsigned char last_ply_was_capture = 64 );

#endif
