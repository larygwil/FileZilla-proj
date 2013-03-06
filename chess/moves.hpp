#ifndef __MOVES_H__
#define __MOVES_H__

#include "chess.hpp"
#include "detect_check.hpp"

#include <vector>

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

enum class movegen_type {
	// Calculates all legal moves.
	all,

	// Returns all legal captures
	// Precondition: Own king not in check
	// Returned evaluation is MVV/LVA
	capture,

	noncapture,
	pseudocheck
};

// Calculates all legal moves
template<enum class movegen_type type>
void calculate_moves( position const& p, move_info*& moves, check_map const& check );

// Calculates all legal moves.
// Do not call in actual search, this function is too slow.
template<enum class movegen_type type>
std::vector<move> calculate_moves( position const& p, check_map const& check );

template<enum class movegen_type type>
inline std::vector<move> calculate_moves( position const& p ) {
	return calculate_moves<type>( p, check_map( p ) );
}

// Calculates legal non-captures
// If only_pseudo_checks is not set,
// all legal noncaptures are returned.
// Otherwise, only legal noncaptures are returned that
// are likely (but not guaranteeded) to give check.
//template<bool only_pseudo_checks>
//void calculate_moves_noncaptures( position const& p, move_info*& moves, check_map const& check );

#endif
