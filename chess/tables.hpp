#ifndef __TABLES_H__
#define __TABLES_H__

#include "platform.hpp"

extern uint64_t const visibility_bishop[64];
extern uint64_t const visibility_rook[64];
extern uint64_t const ray_n[64];
extern uint64_t const ray_e[64];
extern uint64_t const ray_s[64];
extern uint64_t const ray_w[64];
extern uint64_t const ray_ne[64];
extern uint64_t const ray_se[64];
extern uint64_t const ray_sw[64];
extern uint64_t const ray_nw[64];

extern short const proximity[64][64];

extern uint64_t const pawn_control[2][64];

extern uint64_t const possible_knight_moves[64];
extern uint64_t const possible_king_moves[64];


#endif