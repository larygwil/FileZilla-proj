#ifndef __TABLES_H__
#define __TABLES_H__

#include "util/platform.hpp"

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
extern short const king_distance[64][64];

extern uint64_t const pawn_control[2][64];

extern uint64_t const possible_knight_moves[64];
extern uint64_t const possible_king_moves[64];

extern uint64_t const rule_of_the_square[2][2][64];

extern uint64_t const king_attack_zone[2][64];

extern uint64_t const between_squares[64][64];

extern uint64_t const connected_pawns[2][64];
extern uint64_t const isolated_pawns[64];
extern uint64_t const passed_pawns[2][64];
extern uint64_t const doubled_pawns[2][64];
extern uint64_t const forward_pawn_attack[2][64];

extern uint64_t const trapped_bishop[2][64];

#endif
