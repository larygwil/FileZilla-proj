#ifndef __CALC_H__
#define __CALC_H__

#include "config.hpp"
#include "chess.hpp"
#include "detect_check.hpp"
#include "history.hpp"
#include "phased_move_generator.hpp"
#include "pvlist.hpp"
#include "moves.hpp"
#include "seen_positions.hpp"
#include "util.hpp"
#include "statistics.hpp"

#include <sstream>
#include <set>

struct new_best_move_callback_base
{
	virtual void on_new_best_move( unsigned int multipv, position const& p, int depth, int selective_depth, int evaluation, uint64_t nodes, duration const& elapsed, move const* pv ) = 0;

	virtual bool print_only_updated() const { return false; };

};

struct def_new_best_move_callback : public new_best_move_callback_base
{
	def_new_best_move_callback( config const& conf )
		: conf_(conf)
	{
	}

	virtual void on_new_best_move( unsigned int multipv, position const& p, int depth, int selective_depth, int evaluation, uint64_t nodes, duration const& elapsed, move const* pv );

protected:
	config const& conf_;

private:
	std::stringstream ss_;
};

struct null_new_best_move_callback : public new_best_move_callback_base
{
	virtual void on_new_best_move( unsigned int, position const&, int, int, int, uint64_t, duration const&, move const* ) override {}
};

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
	move ponder_move;

	// Extra time spent, e.g. due to changed PV
	duration used_extra_time;
};

class calc_manager
{
public:
	explicit calc_manager( context& ctx );
	virtual ~calc_manager();

	calc_manager( calc_manager const& ) = delete;
	calc_manager& operator=( calc_manager const& ) const = delete;

	// May modify seen_positions at indexes > root_position
	// move_time_limit is the desired time we should calculate.
	// deadline is the maximum time we may calculate without losing the game.
	calc_result calc( position const& p, int depth,
		   timestamp const& start,
		   duration const& move_time_limit, duration const& deadline, int clock,
		   seen_positions const& seen,
		   new_best_move_callback_base& new_best_cb,
		   std::set<move> const& searchmoves = std::set<move>() );

	void clear_abort();
	void abort();
	bool should_abort() const;

	void set_multipv( unsigned int multipv );

#if USE_STATISTICS
	statistics& stats();
#endif
private:
	class impl;
	impl* impl_;
};

class killer_moves
{
public:
	killer_moves() {}

	void add_killer( move const& m, int ply ) {
		if( m_[ply * 2 + 1] != m ) {
			m_[ply * 2] = m_[ply * 2 + 1];
			m_[ply * 2 + 1] = m;
		}
	}

	bool is_killer( move const& m, int ply ) const {
		return m == m_[ply * 2] || m == m_[ply * 2 + 1];
	}

	move m_[(MAX_DEPTH + 1) * 2];
};

class worker_thread;

class calc_state
{
public:
	calc_state()
		: clock(0)
		, move_ptr(moves)
		, do_abort_()
		, thread_()
		, tt_()
		, pawn_tt_()
	{
	}

	unsigned char clock; // The halfmove clock

	move_info moves[200 * (MAX_DEPTH + MAX_QDEPTH)];
	move_info* move_ptr;

	seen_positions seen;

	killer_moves killers[2];

	history history_;

	// Depth is number of plies to search multiplied by depth_factor
	short step( int depth, int ply, position& p, check_map const& check, short alpha, short beta, bool last_was_null, short full_eval = result::win, unsigned char last_ply_was_capture = 64 );

	short inner_step( int const depth, int const ply, position const& p, check_map const& check, short const alpha, short const beta
		, short const full_eval, unsigned char const last_ply_was_capture
		, bool const pv_node, move const& m, unsigned int const processed_moves, phases::type const phase, short const best_value );

	void split( int depth, int ply, position const& p
		, check_map const& check, short alpha, short beta, short full_eval, unsigned char last_ply_was_capture, bool pv_node, short& best_value, move& best_move, phased_move_generator_base& gen );

	short quiescence_search( int ply, int depth, position const& p, check_map const& check, short alpha, short beta, short full_eval = result::win );

	bool do_abort_;

	worker_thread* thread_;

	hash* tt_;
	pawn_structure_hash_table* pawn_tt_;
};

#endif
