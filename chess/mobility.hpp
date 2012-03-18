#ifndef __MOBILITY_H__
#define __MOBILITY_H__

#include <string>

#include "chess.hpp"

score evaluate_mobility( position const& p, color::type c, uint64_t passed_pawns );

std::string explain_eval( position const& p, color::type c );

#endif
