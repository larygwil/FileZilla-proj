#ifndef __EVAL_H__
#define __EVAL_H__

#include "chess.hpp"
#include "score.hpp"

short evaluate_full( position const& p, color::type c );

std::string explain_eval( position const& p, color::type c );

short evaluate_move( position const& p, color::type c, move const& m );

#endif
