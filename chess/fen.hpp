#ifndef __FEN_H__
#define __FEN_H__

#include "chess.hpp"

#include <string>

std::string position_to_fen_noclock( config const& conf, position const& p );

bool parse_fen( config const& conf, std::string const& fen, position&, std::string* error = 0 );

#endif
