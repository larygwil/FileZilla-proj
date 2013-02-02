#ifndef __PVLIST_H__
#define __PVLIST_H__

#include "chess.hpp"

#include <string>

std::string pv_to_string( move const* pv, position p, bool use_long_algebraic_notation = false );
void get_pv_from_tt( move* pv, position p, int max_depth );
void push_pv_to_tt( move const* pv, position p, int clock );

#endif
