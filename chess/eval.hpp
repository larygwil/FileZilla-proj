#ifndef __EVAL_H__
#define __EVAL_H__

#include "chess.hpp"

short evaluate_fast( position const& p, color::type c );

short evaluate_full( position const& p, color::type c );
short evaluate_full( position const& p, color::type c, short eval_fast );

short evaluate_move( position const& p, color::type c, short current_evaluation, move const& m, position::pawn_structure& outPawns );

short evaluate_pawns( unsigned long long const* pawns, color::type c );

namespace material_values {
enum type {
	pawn = 100,
	knight = 295,
	bishop = 305,
	rook = 500,
	queen = 900,
	initial = pawn * 8 + rook * 2 + knight * 2 + bishop * 2 + queen
};
}

short get_material_value( position const& p, color::type c, int pi );

#endif
