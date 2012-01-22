#ifndef __TABLES_H__
#define __TABLES_H__

extern unsigned long long const visibility_bishop[64];
extern unsigned long long const visibility_rook[64];
extern unsigned long long const ray_n[64];
extern unsigned long long const ray_e[64];
extern unsigned long long const ray_s[64];
extern unsigned long long const ray_w[64];
extern unsigned long long const ray_ne[64];
extern unsigned long long const ray_se[64];
extern unsigned long long const ray_sw[64];
extern unsigned long long const ray_nw[64];

extern short const proximity[64][64];

extern unsigned long long const pawn_control[2][64];

extern unsigned long long const possible_knight_moves[64];
extern unsigned long long const possible_king_moves[64];


#endif