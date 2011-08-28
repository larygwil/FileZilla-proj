#ifndef __MOVES_H__
#define __MOVES_H__

#include "chess.hpp"
#include "detect_check.hpp"

struct move_info {
	move m;
	short evaluation;
	position::pawn_structure pawns;
};

void calculate_moves( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check );

// Returns all captures and checks. May return additional moves.
// Precondition: Own king not in check
void calculate_moves_captures_and_checks( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, inverse_check_map const& inverse_check_map );

#endif
