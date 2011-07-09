#ifndef __DETECT_CHECK_H__
#define __DETECT_CHECK_H__

#include "chess.hpp"

void detect_check( position const& p, color::type c, check_info& check, unsigned char king_col, unsigned char king_row );
void detect_check( position const& p, color::type c, check_info& check );

#endif
