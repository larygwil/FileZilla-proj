#ifndef __MOVES_H__
#define __MOVES_H__

#include "chess.hpp"
#include "detect_check.hpp"

class killer_moves;

PACKED(struct move_info,
{
	move m;
	int sort;
});

struct MoveSort {
	inline bool operator()( move_info const& lhs, move_info const& rhs ) const {
		return lhs.sort > rhs.sort;
	}
};
extern MoveSort moveSort;


// Calculates all legal moves
// Returned evaluation is fast_eval
void calculate_moves( position const& p, move_info*& moves, check_map const& check );

// Returns all legal captures
// Precondition: Own king not in check
// Returned evaluation is MVV/LVA
void calculate_moves_captures( position const& p, move_info*& moves, check_map const& check );

// Calculates legal non-captures
// If only_pseudo_checks is not set,
// all legal noncaptures are returned.
// Otherwise, only legal noncaptures are returned that
// are likely (but not guaranteeded) to give check.
template<bool only_pseudo_checks>
void calculate_moves_noncaptures( position const& p, move_info*& moves, check_map const& check );

#endif
