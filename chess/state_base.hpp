#ifndef OCTOCHESS_STATE_BASE_HEADER
#define OCTOCHESS_STATE_BASE_HEADER

#include "move.hpp"
#include "position.hpp"
#include "pv_move_picker.hpp"
#include "random.hpp"
#include "seen_positions.hpp"

#include <set>
#include <vector>

class state_base
{
public:
	state_base();

	virtual void reset( position const& p = position() );

	virtual void apply( move const& m );
	virtual bool undo( unsigned int count );

	position const& p() const { return p_; }

	unsigned int clock() const { return clock_; }

	std::vector<std::pair<position, move>> const& history() const { return history_; }

	seen_positions const& seen() const { return seen_; }

	bool started_from_root() const { return started_from_root_; }

	std::set<move> searchmoves_;
	pv_move_picker pv_move_picker_;
	randgen rng_;
protected:
	position p_;
	std::vector<std::pair<position, move>> history_;
	seen_positions seen_;

	unsigned int clock_;

	bool started_from_root_;
};

#endif