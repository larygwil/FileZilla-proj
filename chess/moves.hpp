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

	// Calculates legal non-captures
	noncapture,

	// As above, but only legal noncaptures are returned that
	// are likely (but not guaranteeded) to give check.
	pseudocheck
};

// Calculates all legal moves
template<movegen_type type>
void calculate_moves( position const& p, move_info*& moves, check_map const& check );

// Calculates all legal moves.
// Do not call in actual search, this function is too slow.
template<movegen_type type>
std::vector<move> calculate_moves( position const& p, check_map const& check );

// Returns all moves by piece type
void calculate_moves_by_piece( position const& p, move_info*& moves, check_map const& check, pieces::type );

template<movegen_type type>
inline std::vector<move> calculate_moves( position const& p ) {
	return calculate_moves<type>( p, check_map( p ) );
}

#endif
