#ifndef __PHASED_MOVE_GENERATOR_H__
#define __PHASED_MOVE_GENERATOR_H__

#include "calc.hpp"

namespace phases {
enum type {
	hash_move,
	captures_gen,
	captures,
	killer1,
	killer2,
	noncaptures_gen,
	noncapture,
	done
};
}


class phased_move_generator_base
{
public:
	phased_move_generator_base( context& cntx, position const& p, color::type const& c, check_map const& check, short const& eval );
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
	color::type const& c_;
	check_map const& check_;
	short const& eval_;
};


// Multi-phase move generator
class qsearch_move_generator : public phased_move_generator_base
{
public:
	qsearch_move_generator( context& cntx, position const& p, color::type const& c, check_map const& check, short const& eval, bool pv_node );

	// Returns the next legal move.
	// move_info's m, evaluation and pawns are filled out, sort is undefined.
	virtual move_info const* next();


private:
	bool pv_node_;
};


class move_generator : public phased_move_generator_base
{
public:
	move_generator( context& cntx, killer_moves const& killers, position const& p, color::type const& c, check_map const& check, short const& eval );

	// Returns the next legal move.
	// move_info's m, evaluation and pawns are filled out, sort is undefined.
	virtual move_info const* next();

private:
	killer_moves const& killers_;
};


#endif
