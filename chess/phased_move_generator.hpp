#ifndef __PHASED_MOVE_GENERATOR_H__
#define __PHASED_MOVE_GENERATOR_H__

#define DELAY_BAD_CAPTURES 1

#include "chess.hpp"

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

class check_map;
class calc_state;
struct move_info;
class position;
class phased_move_generator_base
{
public:
	phased_move_generator_base( calc_state& cntx, position const& p, check_map const& check );
	virtual ~phased_move_generator_base();

	virtual move next() = 0;

	phases::type get_phase() const {
		return phase;
	}

	void set_done() {
		phase = phases::done;
	}

	move hash_move;

protected:
	calc_state& state_;
	phases::type phase;
	move_info* const moves;
	move_info* it;

	position const& p_;
	check_map const& check_;
	move_info* bad_captures_end_;
};


// Multi-phase move generator
class qsearch_move_generator : public phased_move_generator_base
{
public:
	qsearch_move_generator( calc_state& cntx, position const& p, check_map const& check, bool pv_node, bool include_noncaptures );

	// Returns the next legal move.
	// move_info's m, evaluation and pawns are filled out, sort is undefined.
	virtual move next() override;

private:
	bool pv_node_;
	bool include_noncaptures_;
};


class killer_moves;
class move_generator : public phased_move_generator_base
{
public:
	move_generator( calc_state& cntx, int ply, killer_moves const& killers, position const& p, check_map const& check );

	// Returns the next legal move.
	// move_info's m, evaluation and pawns are filled out, sort is undefined.
	virtual move next() override;

	void rewind();
private:
	
	int const ply_;
	killer_moves const& killers_;

#if VERIFY_KILLERS
	move const km1_;
	move const km2_;

	void verify_killers();
#endif
};


#endif
