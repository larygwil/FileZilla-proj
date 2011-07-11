#ifndef __CALC_H__
#define __CALC_H__

#include "chess.hpp"

#include <vector>

int evaluate( color::type c, position const& p, check_info const& check, int depth );
bool calc( position& p, color::type c, move& m, int& res );

void calculate_moves( position const& p, color::type c, std::vector<move>& moves, check_info& check );

#endif
