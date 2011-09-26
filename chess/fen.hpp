#ifndef __FEN_H__
#define __FEN_H__

#include "chess.hpp"

#include <string>

std::string position_to_fen_noclock( position const& p, color::type c );

bool parse_fen_noclock( std::string const& fen, position&, color::type& c );

#endif
