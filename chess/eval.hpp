#ifndef __EVAL_H__
#define __EVAL_H__

#include "chess.hpp"
#include "score.hpp"

short evaluate_full( pawn_structure_hash_table& pawn_tt, position const& p );

std::string explain_eval( pawn_structure_hash_table& pawn_tt, position const& p );

short evaluate_move( position const& p, move const& m );

score evaluate_pawn_shield( position const& p, color::type c );

#endif
