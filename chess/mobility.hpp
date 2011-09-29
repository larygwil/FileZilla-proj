#ifndef __MOBILITY_H__
#define __MOBILITY_H__

#include <string>

#include "chess.hpp"

short evaluate_mobility( position const& p, color::type c );

std::string explain_eval( position const& p, color::type c );

#endif
