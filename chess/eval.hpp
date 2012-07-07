#ifndef __EVAL_H__
#define __EVAL_H__

#include "chess.hpp"
#include "score.hpp"

short evaluate_full( position const& p );

std::string explain_eval( position const& p );

short evaluate_move( position const& p, move const& m );

#endif
