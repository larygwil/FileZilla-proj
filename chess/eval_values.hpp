#ifndef __EVAL_VALUES_H__
#define __EVAL_VALUES_H__

#include "score.hpp"

#define LAZY_EVAL 980

namespace eval_values {
	extern short mg_material_values[7];
	extern short eg_material_values[7];

	extern score double_bishop;

	extern score doubled_pawn;
	extern score passed_pawn;
	extern score isolated_pawn;
	extern score connected_pawn;
	extern score candidate_passed_pawn;

	extern score pawn_shield;

	extern score absolute_pin[7];

	extern score rooks_on_open_file;
	extern score rooks_on_half_open_file;

	extern score connected_rooks;

	extern score tropism[7];

	extern short king_attack_by_piece[6];
	extern short king_check_by_piece[6];
	extern short king_melee_attack_by_rook;
	extern short king_melee_attack_by_queen;

	extern short king_attack_min[2];
	extern short king_attack_max[2];
	extern short king_attack_rise[2];
	extern short king_attack_exponent[2];
	extern short king_attack_offset[2];

	extern score center_control;

	extern short phase_transition_begin;
	extern short phase_transition_duration;

	extern score material_imbalance;

	extern score rule_of_the_square;
	extern score passed_pawn_unhindered;

	extern score hanging_piece[6];

	extern score mobility_knight_min;
	extern score mobility_knight_max;
	extern score mobility_knight_rise;
	extern score mobility_knight_offset;
	extern score mobility_bishop_min;
	extern score mobility_bishop_max;
	extern score mobility_bishop_rise;
	extern score mobility_bishop_offset;
	extern score mobility_rook_min;
	extern score mobility_rook_max;
	extern score mobility_rook_rise;
	extern score mobility_rook_offset;
	extern score mobility_queen_min;
	extern score mobility_queen_max;
	extern score mobility_queen_rise;
	extern score mobility_queen_offset;

	extern score side_to_move;

	extern short drawishness;

	// Derived
	extern score material_values[7];

	extern score initial_material;

	extern int phase_transition_material_begin;
	extern int phase_transition_material_end;

	extern score mobility_knight[9];
	extern score mobility_bishop[14];
	extern score mobility_rook[15];
	extern score mobility_queen[7+7+7+6+1];

	extern score king_attack[200];

	extern short insufficient_material_threshold;

	void init();
	void update_derived();
};

extern score pst[2][7][64];
void init_pst();

#endif
