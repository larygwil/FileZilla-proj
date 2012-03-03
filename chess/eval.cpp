#include "eval.hpp"
#include "assert.hpp"
#include "mobility.hpp"
#include "util.hpp"
#include "pawn_structure_hash_table.hpp"
#include "tables.hpp"
#include "zobrist.hpp"

#include <iostream>

extern unsigned long long const king_pawn_shield[2][64];
extern unsigned long long const isolated_pawns[64];

eval_values_t eval_values;

// Original values
/*eval_values_t::eval_values_t()
{
	material_values[pieces::none] = 0;
	material_values[pieces::pawn] = 100;
	material_values[pieces::knight] = 295;
	material_values[pieces::bishop] = 305;
	material_values[pieces::rook] = 500;
	material_values[pieces::queen] = 900;
	material_values[pieces::king] = 0;

	double_bishop = 25;
	doubled_pawn = -15;
	passed_pawn = 35;
	isolated_pawn = -15;
	connected_pawn = 15;
	pawn_shield = 3;
	castled = 25;

	pin_absolute_bishop = 35;
	pin_absolute_rook = 30;
	pin_absolute_queen = 25;

	mobility_multiplicator = 6;
	mobility_divisor = 3;

	pin_multiplicator = 5;
	pin_divisor = 5;

	rooks_on_open_file_multiplicator = 5;
	rooks_on_open_file_divisor = 5;

	tropism_multiplicator = 5;
	tropism_divisor = 5;

	king_attack_multiplicator = 5;
	king_attack_divisor = 5;

	center_control_multiplicator = 5;
	center_control_divisor = 5;

	update_derived();
}*/

// After much tweaking employing a genetic algorithm, this is better but still far from perfect.
/*eval_values_t::eval_values_t()
{
	material_values[pieces::none] = 0;
	material_values[pieces::pawn] = 100;
	material_values[pieces::knight] = 294;
	material_values[pieces::bishop] = 304;
	material_values[pieces::rook] = 500;
	material_values[pieces::queen] = 900;
	material_values[pieces::king] = 0;

	double_bishop = 28;

	doubled_pawn[0] = -30;
	passed_pawn[0] = 19;
	isolated_pawn[0] = -10;
	connected_pawn[0] = 0;

	doubled_pawn[1] = -30;
	passed_pawn[1] = 19;
	isolated_pawn[1] = -10;
	connected_pawn[1] = 0;

	pawn_shield[0] = 5;
	pawn_shield[1] = 5;

	castled = 25;

	pin_absolute_bishop = 16;
	pin_absolute_rook = 0;
	pin_absolute_queen = 49;

	mobility_multiplicator = 6;
	mobility_divisor = 2;

	pin_multiplicator = 6;
	pin_divisor = 5;

	rooks_on_open_file_multiplicator = 1;
	rooks_on_open_file_divisor = 10;

	tropism_multiplicator = 0;
	tropism_divisor = 5;

	king_attack_multiplicator = 5;
	king_attack_divisor = 1;

	center_control_multiplicator = 8;
	center_control_divisor = 1;

	phase_transition_begin = 600;
	phase_transition_duration = 1200;

	update_derived();
}*/

eval_values_t::eval_values_t()
{
	material_values[pieces::none] = 0;
	material_values[pieces::pawn] = 91;
	material_values[pieces::knight] = 330;
	material_values[pieces::bishop] = 330;
	material_values[pieces::rook] = 524;
	material_values[pieces::queen] = 930;
	material_values[pieces::king] = 20000;

	double_bishop = 35;

	doubled_pawn[0] = -14;
	passed_pawn[0] = 1;
	isolated_pawn[0] = 0;
	connected_pawn[0] = 0;

	doubled_pawn[1] = -24;
	passed_pawn[1] = 44;
	isolated_pawn[1] = -16;
	connected_pawn[1] = 0;

	pawn_shield[0] = 10;
	pawn_shield[1] = 3;

	castled = 0;

	pin_absolute_bishop = 19;
	pin_absolute_rook = 0;
	pin_absolute_queen = 39;

	mobility_multiplicator = 7;
	mobility_divisor = 2;

	pin_multiplicator = 7;
	pin_divisor = 3;

	rooks_on_open_file_multiplicator = 2;
	rooks_on_open_file_divisor = 8;

	tropism_multiplicator = 1;
	tropism_divisor = 9;

	king_attack_multiplicator = 10;
	king_attack_divisor = 1;

	center_control_multiplicator = 8;
	center_control_divisor = 1;

	phase_transition_begin = 983;
	phase_transition_duration = 1999;

	update_derived();
}

void eval_values_t::update_derived()
{
	initial_material = 
		material_values[pieces::pawn] * 8 + 
		material_values[pieces::knight] * 2 +
		material_values[pieces::bishop] * 2 +
		material_values[pieces::rook] * 2 +
		material_values[pieces::queen];

	phase_transition_material_begin = initial_material * 2 - phase_transition_begin;
	phase_transition_material_end = phase_transition_material_begin - phase_transition_duration;
}


short phase_scale( short const* material, short ev1, short ev2 )
{
	int m = material[0] + material[1];
	if( m >= eval_values.phase_transition_material_begin ) {
		return ev1;
	}
	else if( m <= eval_values.phase_transition_material_end ) {
		return ev2;
	}
	
	int position = 256 * (eval_values.phase_transition_material_begin - m) / static_cast<int>(eval_values.phase_transition_duration);
	return ((static_cast<int>(ev1) * position      ) >> 8) +
		   ((static_cast<int>(ev2) * (256-position)) >> 8);
}


namespace {
short const pawn_values[2][64] = {
	{
		 0,  0,  0,   0,   0, 0   , 0   , 0   ,
		 0,  0,  0, -20, -20, 0   , 0   , 0   ,
		 2,  2,  2,   5,   5,   2 ,   2 ,   2 ,
		 3,  5, 10,  20,  20,   10,   5 ,   3 ,
		 6,  8, 17,  27,  27, 17,   8 ,   6 ,
		10, 15, 25,  40,  40, 25,   15,   10,
		45, 50, 55,  60,  60, 55,   50,   45,
		 0,  0,  0,   0,   0,  0   , 0   , 0
	},
	{
		 0, 0   , 0   , 0   , 0   , 0   , 0   , 0   ,
		45,   50,   55,   60,   60,   55,   50,   45,
		10,   15,   25,   40,   40,   25,   15,   10 ,
		 5,   8 ,   17,   27,   27,   17,   8 ,   6 ,
		 3,   5 ,   10,   20,   20,   10,   5 ,   3 ,
		 2,   2 ,   2 ,   5 ,   5 ,   2 ,   2 ,   2 ,
		 0, 0   , 0   ,   20,  20, 0   , 0   , 0   ,
		 0, 0   , 0   , 0   , 0   , 0   , 0   , 0
	}
};


short const queen_values[2][64] = {
	{
		 -20,  -10,  -10,  -5 ,  -5 ,  -10,  -10,  -20,
		 -10,    0,   2 ,   3 ,   3 ,   2 , 0   ,  -10,
		 -10,    2,   5 ,   5 ,   5 ,   5 ,   2 ,  -10,
		  -5,    3,   5 ,   7 ,   7 ,   5 ,   3 ,  -5 ,
		  -5,    3,   5 ,   7 ,   7 ,   5 ,   3 ,  -5 ,
		 -10,    2,   5 ,   5 ,   5 ,   5 ,   2 ,  -10,
		 -10, 0   ,   2 ,   3 ,   3 ,   2 , 0   ,  -10,
		 -20,  -10,  -10,  -5 ,  -5 ,  -10,  -10,  -20,
	},
	{
		 -20,  -10,  -10,  -5 ,  -5 ,  -10,  -10,  -20,
		 -10, 0   ,   2 ,   3 ,   3 ,   2 , 0   ,  -10,
		 -10,   2 ,   5 ,   5 ,   5 ,   5 ,   2 ,  -10,
		 -5 ,   3 ,   5 ,   7 ,   7 ,   5 ,   3 ,  -5 ,
		 -5 ,   3 ,   5 ,   7 ,   7 ,   5 ,   3 ,  -5 ,
		 -10,   2 ,   5 ,   5 ,   5 ,   5 ,   2 ,  -10,
		 -10, 0   ,   2 ,   3 ,   3 ,   2 , 0   ,  -10,
		 -20,  -10,  -10,  -5 ,  -5 ,  -10,  -10,  -20,
	}
};

short const rook_values[2][64] = {
	{
		0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
		 -5, 0  , 0  , 0  , 0  , 0  , 0  , 5,
		 -5, 0  , 0  , 0  , 0  , 0  , 0  , 5,
		 -5, 0  , 0  , 0  , 0  , 0  , 0  , 5,
		 -5, 0  , 0  , 0  , 0  , 0  , 0  , 5,
		 -5, 0  , 0  , 0  , 0  , 0  , 0  , 5,
		  5,   5,   5,   5,   5,   5,   5,   5,
		0  , 0  , 0  , 0  , 0  , 0  , 0  , 0
	},
	{
		0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
		  5,   5,   5,   5,   5,   5,   5,   5,
		 -5, 0  , 0  , 0  , 0  , 0  , 0  ,  -5,
		 -5, 0  , 0  , 0  , 0  , 0  , 0  ,  -5,
		 -5, 0  , 0  , 0  , 0  , 0  , 0  ,  -5,
		 -5, 0  , 0  , 0  , 0  , 0  , 0  ,  -5,
		 -5, 0  , 0  , 0  , 0  , 0  , 0  ,  -5,
		0  , 0  , 0  , 0  , 0  , 0  , 0  , 0  ,
	}
};

short const knight_values[2][64] = {
	{
		 -25,  -15,  -10,  -10,  -10,  -10,  -15,  -25,
		 -15,  -5,  0,      2,    2,  0,     -5,   -15,
		 -10, 0,      2,    5,    5,    2,  0,     -10,
		 -10,   2,    5,    10,   10,   5,    2,   -10,
		 -10,   2,    5,    10,   10,   5,    2,   -10,
		 -10, 0,      2,    5,    5,    2,  0,     -10,
		 -15,  -5,  0,      2,    2,  0,     -5,   -15,
		 -25,  -15,  -10,  -10,  -10,  -10,  -15,  -25
	},
	{
		 -25,  -15,  -10,  -10,  -10,  -10,  -15,  -25,
		 -15,  -5,  0,      2,    2,  0,     -5,   -15,
		 -10, 0,      2,    5,    5,    2,  0,     -10,
		 -10,   2,    5,    10,   10,   5,    2,   -10,
		 -10,   2,    5,    10,   10,   5,    2,   -10,
		 -10, 0,      2,    5,    5,    2,  0,     -10,
		 -15,  -5,  0,      2,    2,  0,     -5,   -15,
		 -25,  -15,  -10,  -10,  -10,  -10,  -15,  -25
	}
};

short const bishop_values[2][64] = {
	{
		 -10,  -9,   -7,   -5,   -5,   -7,   -9,   -10,
		 -2,    10,   5,    10,   10,   5,    10,  -2,
		  3,    6,    15,   12,   12,   15,   6,    3,
		  5,    10,   12,   20,   20,   12,   10,   5,
		  5,    10,   12,   20,   20,   12,   10,   5,
		  3,    6,    15,   12,   12,   15,   6,    3,
		0,      10,   5,    10,   10,   5,    10, 0,
		0,    0,      2,    5,    5,    2,  0,    0
	},
	{
		0,    0,      2,    5,    5,    2,  0,    0,
		0,      10,   5,    10,   10,   5,    10, 0,
		  3,    6,    15,   12,   12,   15,   6,    3,
		  5,    10,   12,   20,   20,   12,   10,   5,
		  5,    10,   12,   20,   20,   12,   10,   5,
		  3,    6,    15,   12,   12,   15,   6,    3,
		 -2,    10,   5,    10,   10,   5,    10,  -2,
		 -10,  -9,   -7,   -5,   -5,   -7,   -9,   -10
	}
};

unsigned long long const passed_pawns[2][64] = {
{
		0x0303030303030300ull,
		0x0707070707070700ull,
		0x0e0e0e0e0e0e0e00ull,
		0x1c1c1c1c1c1c1c00ull,
		0x3838383838383800ull,
		0x7070707070707000ull,
		0xe0e0e0e0e0e0e000ull,
		0xc0c0c0c0c0c0c000ull,
		0x0303030303030000ull,
		0x0707070707070000ull,
		0x0e0e0e0e0e0e0000ull,
		0x1c1c1c1c1c1c0000ull,
		0x3838383838380000ull,
		0x7070707070700000ull,
		0xe0e0e0e0e0e00000ull,
		0xc0c0c0c0c0c00000ull,
		0x0303030303000000ull,
		0x0707070707000000ull,
		0x0e0e0e0e0e000000ull,
		0x1c1c1c1c1c000000ull,
		0x3838383838000000ull,
		0x7070707070000000ull,
		0xe0e0e0e0e0000000ull,
		0xc0c0c0c0c0000000ull,
		0x0303030300000000ull,
		0x0707070700000000ull,
		0x0e0e0e0e00000000ull,
		0x1c1c1c1c00000000ull,
		0x3838383800000000ull,
		0x7070707000000000ull,
		0xe0e0e0e000000000ull,
		0xc0c0c0c000000000ull,
		0x0303030000000000ull,
		0x0707070000000000ull,
		0x0e0e0e0000000000ull,
		0x1c1c1c0000000000ull,
		0x3838380000000000ull,
		0x7070700000000000ull,
		0xe0e0e00000000000ull,
		0xc0c0c00000000000ull,
		0x0303000000000000ull,
		0x0707000000000000ull,
		0x0e0e000000000000ull,
		0x1c1c000000000000ull,
		0x3838000000000000ull,
		0x7070000000000000ull,
		0xe0e0000000000000ull,
		0xc0c0000000000000ull,
		0x0300000000000000ull,
		0x0700000000000000ull,
		0x0e00000000000000ull,
		0x1c00000000000000ull,
		0x3800000000000000ull,
		0x7000000000000000ull,
		0xe000000000000000ull,
		0xc000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull
	},
{
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000003ull,
		0x0000000000000007ull,
		0x000000000000000eull,
		0x000000000000001cull,
		0x0000000000000038ull,
		0x0000000000000070ull,
		0x00000000000000e0ull,
		0x00000000000000c0ull,
		0x0000000000000303ull,
		0x0000000000000707ull,
		0x0000000000000e0eull,
		0x0000000000001c1cull,
		0x0000000000003838ull,
		0x0000000000007070ull,
		0x000000000000e0e0ull,
		0x000000000000c0c0ull,
		0x0000000000030303ull,
		0x0000000000070707ull,
		0x00000000000e0e0eull,
		0x00000000001c1c1cull,
		0x0000000000383838ull,
		0x0000000000707070ull,
		0x0000000000e0e0e0ull,
		0x0000000000c0c0c0ull,
		0x0000000003030303ull,
		0x0000000007070707ull,
		0x000000000e0e0e0eull,
		0x000000001c1c1c1cull,
		0x0000000038383838ull,
		0x0000000070707070ull,
		0x00000000e0e0e0e0ull,
		0x00000000c0c0c0c0ull,
		0x0000000303030303ull,
		0x0000000707070707ull,
		0x0000000e0e0e0e0eull,
		0x0000001c1c1c1c1cull,
		0x0000003838383838ull,
		0x0000007070707070ull,
		0x000000e0e0e0e0e0ull,
		0x000000c0c0c0c0c0ull,
		0x0000030303030303ull,
		0x0000070707070707ull,
		0x00000e0e0e0e0e0eull,
		0x00001c1c1c1c1c1cull,
		0x0000383838383838ull,
		0x0000707070707070ull,
		0x0000e0e0e0e0e0e0ull,
		0x0000c0c0c0c0c0c0ull,
		0x0003030303030303ull,
		0x0007070707070707ull,
		0x000e0e0e0e0e0e0eull,
		0x001c1c1c1c1c1c1cull,
		0x0038383838383838ull,
		0x0070707070707070ull,
		0x00e0e0e0e0e0e0e0ull,
		0x00c0c0c0c0c0c0c0ull
	}
};
unsigned long long const doubled_pawns[2][64] = {
	{
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000001ull,
		0x0000000000000002ull,
		0x0000000000000004ull,
		0x0000000000000008ull,
		0x0000000000000010ull,
		0x0000000000000020ull,
		0x0000000000000040ull,
		0x0000000000000080ull,
		0x0000000000000101ull,
		0x0000000000000202ull,
		0x0000000000000404ull,
		0x0000000000000808ull,
		0x0000000000001010ull,
		0x0000000000002020ull,
		0x0000000000004040ull,
		0x0000000000008080ull,
		0x0000000000010101ull,
		0x0000000000020202ull,
		0x0000000000040404ull,
		0x0000000000080808ull,
		0x0000000000101010ull,
		0x0000000000202020ull,
		0x0000000000404040ull,
		0x0000000000808080ull,
		0x0000000001010101ull,
		0x0000000002020202ull,
		0x0000000004040404ull,
		0x0000000008080808ull,
		0x0000000010101010ull,
		0x0000000020202020ull,
		0x0000000040404040ull,
		0x0000000080808080ull,
		0x0000000101010101ull,
		0x0000000202020202ull,
		0x0000000404040404ull,
		0x0000000808080808ull,
		0x0000001010101010ull,
		0x0000002020202020ull,
		0x0000004040404040ull,
		0x0000008080808080ull,
		0x0000010101010101ull,
		0x0000020202020202ull,
		0x0000040404040404ull,
		0x0000080808080808ull,
		0x0000101010101010ull,
		0x0000202020202020ull,
		0x0000404040404040ull,
		0x0000808080808080ull,
		0x0001010101010101ull,
		0x0002020202020202ull,
		0x0004040404040404ull,
		0x0008080808080808ull,
		0x0010101010101010ull,
		0x0020202020202020ull,
		0x0040404040404040ull,
		0x0080808080808080ull
	},
	{
		0x0101010101010100ull,
		0x0202020202020200ull,
		0x0404040404040400ull,
		0x0808080808080800ull,
		0x1010101010101000ull,
		0x2020202020202000ull,
		0x4040404040404000ull,
		0x8080808080808000ull,
		0x0101010101010000ull,
		0x0202020202020000ull,
		0x0404040404040000ull,
		0x0808080808080000ull,
		0x1010101010100000ull,
		0x2020202020200000ull,
		0x4040404040400000ull,
		0x8080808080800000ull,
		0x0101010101000000ull,
		0x0202020202000000ull,
		0x0404040404000000ull,
		0x0808080808000000ull,
		0x1010101010000000ull,
		0x2020202020000000ull,
		0x4040404040000000ull,
		0x8080808080000000ull,
		0x0101010100000000ull,
		0x0202020200000000ull,
		0x0404040400000000ull,
		0x0808080800000000ull,
		0x1010101000000000ull,
		0x2020202000000000ull,
		0x4040404000000000ull,
		0x8080808000000000ull,
		0x0101010000000000ull,
		0x0202020000000000ull,
		0x0404040000000000ull,
		0x0808080000000000ull,
		0x1010100000000000ull,
		0x2020200000000000ull,
		0x4040400000000000ull,
		0x8080800000000000ull,
		0x0101000000000000ull,
		0x0202000000000000ull,
		0x0404000000000000ull,
		0x0808000000000000ull,
		0x1010000000000000ull,
		0x2020000000000000ull,
		0x4040000000000000ull,
		0x8080000000000000ull,
		0x0100000000000000ull,
		0x0200000000000000ull,
		0x0400000000000000ull,
		0x0800000000000000ull,
		0x1000000000000000ull,
		0x2000000000000000ull,
		0x4000000000000000ull,
		0x8000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull
	}
};
unsigned long long const connected_pawns[2][64] = {
{
		0x0000000000000202ull,
		0x0000000000000505ull,
		0x0000000000000a0aull,
		0x0000000000001414ull,
		0x0000000000002828ull,
		0x0000000000005050ull,
		0x000000000000a0a0ull,
		0x0000000000004040ull,
		0x0000000000020202ull,
		0x0000000000050505ull,
		0x00000000000a0a0aull,
		0x0000000000141414ull,
		0x0000000000282828ull,
		0x0000000000505050ull,
		0x0000000000a0a0a0ull,
		0x0000000000404040ull,
		0x0000000002020200ull,
		0x0000000005050500ull,
		0x000000000a0a0a00ull,
		0x0000000014141400ull,
		0x0000000028282800ull,
		0x0000000050505000ull,
		0x00000000a0a0a000ull,
		0x0000000040404000ull,
		0x0000000202020000ull,
		0x0000000505050000ull,
		0x0000000a0a0a0000ull,
		0x0000001414140000ull,
		0x0000002828280000ull,
		0x0000005050500000ull,
		0x000000a0a0a00000ull,
		0x0000004040400000ull,
		0x0000020202000000ull,
		0x0000050505000000ull,
		0x00000a0a0a000000ull,
		0x0000141414000000ull,
		0x0000282828000000ull,
		0x0000505050000000ull,
		0x0000a0a0a0000000ull,
		0x0000404040000000ull,
		0x0002020200000000ull,
		0x0005050500000000ull,
		0x000a0a0a00000000ull,
		0x0014141400000000ull,
		0x0028282800000000ull,
		0x0050505000000000ull,
		0x00a0a0a000000000ull,
		0x0040404000000000ull,
		0x0202020000000000ull,
		0x0505050000000000ull,
		0x0a0a0a0000000000ull,
		0x1414140000000000ull,
		0x2828280000000000ull,
		0x5050500000000000ull,
		0xa0a0a00000000000ull,
		0x4040400000000000ull,
		0x0202000000000000ull,
		0x0505000000000000ull,
		0x0a0a000000000000ull,
		0x1414000000000000ull,
		0x2828000000000000ull,
		0x5050000000000000ull,
		0xa0a0000000000000ull,
		0x4040000000000000ull
	},
{
		0x0000000000000202ull,
		0x0000000000000505ull,
		0x0000000000000a0aull,
		0x0000000000001414ull,
		0x0000000000002828ull,
		0x0000000000005050ull,
		0x000000000000a0a0ull,
		0x0000000000004040ull,
		0x0000000000020202ull,
		0x0000000000050505ull,
		0x00000000000a0a0aull,
		0x0000000000141414ull,
		0x0000000000282828ull,
		0x0000000000505050ull,
		0x0000000000a0a0a0ull,
		0x0000000000404040ull,
		0x0000000002020200ull,
		0x0000000005050500ull,
		0x000000000a0a0a00ull,
		0x0000000014141400ull,
		0x0000000028282800ull,
		0x0000000050505000ull,
		0x00000000a0a0a000ull,
		0x0000000040404000ull,
		0x0000000202020000ull,
		0x0000000505050000ull,
		0x0000000a0a0a0000ull,
		0x0000001414140000ull,
		0x0000002828280000ull,
		0x0000005050500000ull,
		0x000000a0a0a00000ull,
		0x0000004040400000ull,
		0x0000020202000000ull,
		0x0000050505000000ull,
		0x00000a0a0a000000ull,
		0x0000141414000000ull,
		0x0000282828000000ull,
		0x0000505050000000ull,
		0x0000a0a0a0000000ull,
		0x0000404040000000ull,
		0x0002020200000000ull,
		0x0005050500000000ull,
		0x000a0a0a00000000ull,
		0x0014141400000000ull,
		0x0028282800000000ull,
		0x0050505000000000ull,
		0x00a0a0a000000000ull,
		0x0040404000000000ull,
		0x0202020000000000ull,
		0x0505050000000000ull,
		0x0a0a0a0000000000ull,
		0x1414140000000000ull,
		0x2828280000000000ull,
		0x5050500000000000ull,
		0xa0a0a00000000000ull,
		0x4040400000000000ull,
		0x0202000000000000ull,
		0x0505000000000000ull,
		0x0a0a000000000000ull,
		0x1414000000000000ull,
		0x2828000000000000ull,
		0x5050000000000000ull,
		0xa0a0000000000000ull,
		0x4040000000000000ull
	}
};


void evaluate_pawn( unsigned long long own_pawns, unsigned long long foreign_pawns, color::type c, unsigned long long pawn,
					 unsigned long long& unpassed, unsigned long long& doubled, unsigned long long& connected, unsigned long long& unisolated )
{
	doubled |= doubled_pawns[c][pawn] & own_pawns;
	unpassed |= passed_pawns[c][pawn] & foreign_pawns;
	connected |= connected_pawns[c][pawn] & own_pawns;
	unisolated |= isolated_pawns[pawn] & own_pawns;
}
}

void evaluate_pawns( unsigned long long white_pawns, unsigned long long black_pawns, short* eval )
{
	// Two while loops, otherwise nice branchless solution.

	unsigned long long unpassed_white = 0;
	unsigned long long doubled_white = 0;
	unsigned long long connected_white = 0;
	unsigned long long unisolated_white = 0;
	unsigned long long unpassed_black = 0;
	unsigned long long doubled_black = 0;
	unsigned long long connected_black = 0;
	unsigned long long unisolated_black = 0;
	{

		unsigned long long pawns = white_pawns;
		while( pawns ) {
			unsigned long long pawn = bitscan_unset( pawns );

			evaluate_pawn( white_pawns, black_pawns, color::white, pawn,
								  unpassed_black, doubled_white, connected_white, unisolated_white );

		}
	}


	{
		unsigned long long pawns = black_pawns;
		while( pawns ) {
			unsigned long long pawn = bitscan_unset( pawns );

			evaluate_pawn( black_pawns, white_pawns, color::black, pawn,
						   unpassed_white, doubled_black, connected_black, unisolated_black );

		}
	}
	unpassed_white |= doubled_white;
	unpassed_black |= doubled_black;

	eval[0] = static_cast<short>(eval_values.passed_pawn[0] * popcount(white_pawns ^ unpassed_white));
	eval[0] += static_cast<short>(eval_values.doubled_pawn[0] * popcount(doubled_white));
	eval[0] += static_cast<short>(eval_values.connected_pawn[0] * popcount(connected_white));
	eval[0] += static_cast<short>(eval_values.isolated_pawn[0] * popcount(white_pawns ^ unisolated_white));
	eval[0] -= static_cast<short>(eval_values.passed_pawn[0] * popcount(black_pawns ^ unpassed_black));
	eval[0] -= static_cast<short>(eval_values.doubled_pawn[0] * popcount(doubled_black));
	eval[0] -= static_cast<short>(eval_values.connected_pawn[0] * popcount(connected_black));
	eval[0] -= static_cast<short>(eval_values.isolated_pawn[0] * popcount(black_pawns ^ unisolated_black));

	eval[1] = static_cast<short>(eval_values.passed_pawn[1] * popcount(white_pawns ^ unpassed_white));
	eval[1] += static_cast<short>(eval_values.doubled_pawn[1] * popcount(doubled_white));
	eval[1] += static_cast<short>(eval_values.connected_pawn[1] * popcount(connected_white));
	eval[1] += static_cast<short>(eval_values.isolated_pawn[1] * popcount(white_pawns ^ unisolated_white));
	eval[1] -= static_cast<short>(eval_values.passed_pawn[1] * popcount(black_pawns ^ unpassed_black));
	eval[1] -= static_cast<short>(eval_values.doubled_pawn[1] * popcount(doubled_black));
	eval[1] -= static_cast<short>(eval_values.connected_pawn[1] * popcount(connected_black));
	eval[1] -= static_cast<short>(eval_values.isolated_pawn[1] * popcount(black_pawns ^ unisolated_black));
}


short evaluate_side( position const& p, color::type c )
{
	short result = 0;

	unsigned long long all_pieces = p.bitboards[c].b[bb_type::all_pieces];
	while( all_pieces ) {
		unsigned long long piece = bitscan_unset( all_pieces );

		unsigned long long bpiece = 1ull << piece;
		if( p.bitboards[c].b[bb_type::pawns] & bpiece ) {
			result += pawn_values[c][piece] + eval_values.material_values[pieces::pawn];
		}
		else if( p.bitboards[c].b[bb_type::knights] & bpiece ) {
			result += knight_values[c][piece] + eval_values.material_values[pieces::knight];
		}
		else if( p.bitboards[c].b[bb_type::bishops] & bpiece ) {
			result += bishop_values[c][piece] + eval_values.material_values[pieces::bishop];
		}
		else if( p.bitboards[c].b[bb_type::rooks] & bpiece ) {
			result += rook_values[c][piece] + eval_values.material_values[pieces::rook];
		}
		else if( p.bitboards[c].b[bb_type::queens] & bpiece ) {
			result += queen_values[c][piece] + eval_values.material_values[pieces::queen];
		}
	}

	if( popcount( p.bitboards[c].b[bb_type::bishops] ) >= 2 ) {
		result += eval_values.double_bishop;
	}

	if( p.castle[c] & castles::has_castled ) {
		result += eval_values.castled;
	}

	return result;
}

short evaluate_fast( position const& p, color::type c )
{
	int value = evaluate_side( p, c ) - evaluate_side( p, static_cast<color::type>(1-c) );

	short pawn_eval[2];
	evaluate_pawns( p.bitboards[0].b[bb_type::pawns], p.bitboards[1].b[bb_type::pawns], pawn_eval );
	short scaled_pawns = phase_scale( p.material, pawn_eval[0], pawn_eval[1] );
	if( c ) {
		value -= scaled_pawns;
	}
	else {
		value += scaled_pawns;
	}

	ASSERT( value > result::loss && value < result::win );

	return value;
}

namespace {
static short get_piece_square_value( color::type c, pieces::type target, unsigned char pi )
{
	switch( target ) {
	case pieces::pawn:
		return pawn_values[c][pi] + eval_values.material_values[pieces::pawn];
	case pieces::knight:
		return knight_values[c][pi] + eval_values.material_values[pieces::knight];
	case pieces::bishop:
		return bishop_values[c][pi] + eval_values.material_values[pieces::bishop];
	case pieces::rook:
		return rook_values[c][pi] + eval_values.material_values[pieces::rook];
	case pieces::queen:
		return queen_values[c][pi] + eval_values.material_values[pieces::queen];
	default:
	case pieces::king:
		return 0;
	}
}
}


short evaluate_move( position const& p, color::type c, short current_evaluation, move const& m, position::pawn_structure& outPawns )
{
#if 0
	short old_eval = current_evaluation;
	{
		short fresh_eval = evaluate_fast( p, c );
		if( fresh_eval != current_evaluation ) {
			std::cerr << "BAD" << std::endl;
		}
	}
#endif

	if( m.flags & move_flags::castle ) {
		
		current_evaluation += eval_values.castled;
		unsigned char row = c ? 56 : 0;
		if( m.target % 8 == 6 ) {
			// Kingside
			current_evaluation -= rook_values[c][7 + row];
			current_evaluation += rook_values[c][5 + row];
		}
		else {
			// Queenside
			current_evaluation -= rook_values[c][0 + row];
			current_evaluation += rook_values[c][3 + row];
		}

		outPawns = p.pawns;

		return current_evaluation;
	}

	if( m.captured_piece != pieces::none ) {
		if( m.flags & move_flags::enpassant ) {
			unsigned char ep = (m.target & 0x7) | (m.source & 0x38);
			current_evaluation += pawn_values[1-c][ep] + eval_values.material_values[pieces::pawn];
		}
		else {
			current_evaluation += get_piece_square_value( static_cast<color::type>(1-c), m.captured_piece, m.target );
		}

		if( m.captured_piece == pieces::bishop && popcount( p.bitboards[1-c].b[bb_type::bishops] ) == 2 ) {
			current_evaluation += eval_values.double_bishop;
		}
	}

	if( m.flags & move_flags::promotion ) {
		current_evaluation -= pawn_values[c][m.source] + eval_values.material_values[pieces::pawn];
		switch( m.promotion ) {
			case promotions::knight:
				current_evaluation += knight_values[c][m.target];
				break;
			case promotions::bishop:
				current_evaluation += bishop_values[c][m.target];
				if( popcount( p.bitboards[c].b[bb_type::bishops] ) == 1 ) {
					current_evaluation += eval_values.double_bishop;
				}
				break;
			case promotions::rook:
				current_evaluation += rook_values[c][m.target];
				break;
			case promotions::queen:
				current_evaluation += queen_values[c][m.target];
				break;
		}
		current_evaluation += eval_values.material_values[pieces::knight + m.promotion];
	}
	else {
		current_evaluation -= get_piece_square_value( c, m.piece, m.source );
		current_evaluation += get_piece_square_value( c, m.piece, m.target );
	}

	outPawns = p.pawns;
	if( m.piece == pieces::pawn || m.captured_piece == pieces::pawn ) {
		short material[2];
		material[0] = p.material[0];
		material[1] = p.material[1];

		if( m.captured_piece != pieces::none ) {
			material[1-c] -= get_material_value( m.captured_piece );
		}
		if( m.flags & move_flags::promotion ) {
			material[c] -= get_material_value( pieces::pawn );
			material[c] += get_material_value( static_cast<pieces::type>(pieces::knight + m.promotion) );
		}

		unsigned long long pawnMap[2];
		pawnMap[0] = p.bitboards[0].b[bb_type::pawns];
		pawnMap[1] = p.bitboards[1].b[bb_type::pawns];

		if( m.captured_piece == pieces::pawn ) {
			if( m.flags & move_flags::enpassant ) {
				unsigned char ep = (m.target & 0x7) | (m.source & 0x38);
				pawnMap[1-c] &= ~(1ull << ep );
				outPawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(1-c), ep );
			}
			else {
				pawnMap[1-c] &= ~(1ull << m.target );
				outPawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(1-c), m.target );
			}
		}
		if( m.piece == pieces::pawn ) {
			pawnMap[c] &= ~(1ull << m.source );
			outPawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.source );
			if( !(m.flags & move_flags::promotion) ) {
				outPawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.target );
				pawnMap[c] |= 1ull << m.target;
			}
		}

		short pawn_eval[2];
		if( !pawn_hash_table.lookup( outPawns.hash, pawn_eval ) ) {
			evaluate_pawns(pawnMap[0], pawnMap[1], pawn_eval);
			pawn_hash_table.store( outPawns.hash, pawn_eval );
		}

		short scaled_eval = phase_scale( material, pawn_eval[0], pawn_eval[1] );
		if( c == color::white ) {
			current_evaluation -= outPawns.eval;
			current_evaluation += scaled_eval;
		}
		else {
			current_evaluation += outPawns.eval;
			current_evaluation -= scaled_eval;
		}
		outPawns.eval = scaled_eval;
	}

#if 0
	{
		position p2 = p;
		p2.init_pawn_structure();

		position p4 = p;
		p4.init_pawn_structure();

		bool captured;
		apply_move( p2, m, c, captured );
		short fresh_eval = evaluate_fast( p2, c );
		if( fresh_eval != current_evaluation ) {
			std::cerr << "BAD" << std::endl;

			position p3 = p;
			bool captured;
			apply_move( p3, m, c, captured );

			evaluate_move( p, c, evaluate_fast( p, c ), m, outPawns );
		}
	}
#endif

	return current_evaluation;
}


/* Pawn shield for king
 * 1 2 3
 * 4 5 6
 *   K
 * Pawns on 1, 3, 4, 6 are awarded shield_const points, pawns on 2 and 5 are awarded twice as many points.
 * Double pawns do not count.
 *
 * Special case: a and h file. There b and g also count twice.
 */
static void evaluate_pawn_shield_side( position const& p, color::type c, short* pawn_shield )
{
	unsigned long long kings = p.bitboards[c].b[bb_type::king];
	unsigned long long king = bitscan( kings );

	unsigned long long shield = king_pawn_shield[c][king] & p.bitboards[c].b[bb_type::pawns];
	pawn_shield[0] = static_cast<short>(eval_values.pawn_shield[0] * popcount(shield));
	pawn_shield[1] = static_cast<short>(eval_values.pawn_shield[1] * popcount(shield));
}


void evaluate_pawn_shield( position const& p, color::type c, short* pawn_shield )
{
	short own[2];
	short other[2];

	evaluate_pawn_shield_side( p, c, own );
	evaluate_pawn_shield_side( p, static_cast<color::type>(1-c), other );

	pawn_shield[0] = own[0] - other[0];
	pawn_shield[1] = own[1] - other[1];
}

short evaluate_full( position const& p, color::type c )
{
	short eval = evaluate_fast( p, c );

	return evaluate_full( p, c, eval );
}

short evaluate_full( position const& p, color::type c, short eval_fast )
{
	short pawn_shield[2];
	evaluate_pawn_shield( p, c, pawn_shield );

	eval_fast += phase_scale( p.material, pawn_shield[0], pawn_shield[1] );
	
	eval_fast += evaluate_mobility( p, c );
	
	// Adjust score based on material. The basic idea is that,
	// given two positions with equal, non-zero score,
	// the position having fewer material is better.
	short v = 25 * (std::max)( 0, eval_values.initial_material * 2 - p.material[0] - p.material[1] ) / (eval_values.initial_material * 2);
	if( eval_fast > 0 ) {
		eval_fast += v;
	}
	else if( eval_fast < 0 ) {
		eval_fast -= v;
	}

	return eval_fast;
}

short get_material_value( pieces::type pi )
{
	return eval_values.material_values[pi];
}
