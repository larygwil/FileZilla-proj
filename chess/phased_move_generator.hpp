#ifndef __PHASED_MOVE_GENERATOR_H__
#define __PHASED_MOVE_GENERATOR_H__

#include "calc.hpp"

#define DELAY_BAD_CAPTURES 1

namespace phases {
enum type {
	hash_move,
	captures_gen,
	captures,
	killer1,
	killer2,
	noncaptures_gen,
	noncapture,
#if DELAY_BAD_CAPTURES
	bad_captures,
#endif
	done
};
}


class phased_move_generator_base
{
public:
	phased_move_generator_base( context& cntx, position const& p, check_map const& check );
	virtual ~phased_move_generator_base();

	virtual move_info const* next() = 0;

	phases::type get_phase() const {
		return phase;
	}

	move hash_move;

protected:
	context& ctx;
	phases::type phase;
	move_info* moves;
	move_info* it;

	position const& p_;
	check_map const& check_;
	move_info* bad_captures_end_;
};


// Multi-phase move generator
class qsearch_move_generator : public phased_move_generator_base
{
public:
	qsearch_move_generator( context& cntx, position const& p, check_map const& check, bool pv_node, bool include_noncaptures );

	// Returns the next legal move.
	// move_info's m, evaluation and pawns are filled out, sort is undefined.
	virtual move_info const* next();

private:
	bool pv_node_;
	bool include_noncaptures_;
};


class move_generator : public phased_move_generator_base
{
public:
	move_generator( context& cntx, killer_moves const& killers, position const& p, check_map const& check );

	// Returns the next legal move.
	// move_info's m, evaluation and pawns are filled out, sort is undefined.
	virtual move_info const* next();

	void update_history();
private:
	killer_moves const& killers_;
};


#endif
