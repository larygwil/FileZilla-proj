#ifndef __EVAL_H__
#define __EVAL_H__

#include "chess.hpp"

short evaluate( position const& p, color::type c );

short evaluate_move( position const& p, color::type c, short current_evaluation, move const& m );

#endif
