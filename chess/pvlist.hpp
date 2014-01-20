#ifndef __PVLIST_H__
#define __PVLIST_H__

#include "chess.hpp"

#include <string>

std::string pv_to_string( config const& conf, move const* pv, position p, bool use_long_algebraic_notation = false );
short get_pv_from_tt( hash& tt, move* pv, position p, int max_depth );
void push_pv_to_tt( hash& tt, move const* pv, position p, int clock );

#endif
