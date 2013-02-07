#ifndef __EVAL_VALUES_H__
#define __EVAL_VALUES_H__

#include "score.hpp"

#define LAZY_EVAL 980

namespace eval_values {
	extern score material_values[7];

	extern score double_bishop;

	extern score passed_pawn_advance_power;
	extern score passed_pawn_king_distance[2];
	extern score doubled_pawn_base[2][4];
	extern score passed_pawn_base[4];
	extern score isolated_pawn_base[2][4];
	extern score connected_pawn_base[2][4];
	extern score candidate_passed_pawn_base[4];
	extern score backward_pawn_base[2][4];

	extern score pawn_shield[3];
	extern score pawn_shield_attack[3];

	extern score absolute_pin[7];

	extern score rooks_on_open_file;
	extern score rooks_on_half_open_file;

	extern score connected_rooks;

	extern score tropism[7];

	extern short king_attack_by_piece[6];
	extern short king_check_by_piece[6];
	extern short king_melee_attack_by_rook;
	extern short king_melee_attack_by_queen;

	extern short king_attack_pawn_shield;

	extern score center_control;

	extern short phase_transition_begin;
	extern short phase_transition_duration;

	extern score material_imbalance[2];

	extern score rule_of_the_square;
	extern score passed_pawn_unhindered;

	extern score defended_by_pawn[6];
	extern score attacked_piece[6];
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

	extern score rooks_on_rank_7;

	extern score knight_outposts[2];
	extern score bishop_outposts[2];

	extern score trapped_rook[2];

	// Derived
	extern score initial_material;

	extern int phase_transition_material_begin;
	extern int phase_transition_material_end;

	extern score doubled_pawn[2][8];
	extern score passed_pawn[8];
	extern score isolated_pawn[2][8];
	extern score connected_pawn[2][8];
	extern score candidate_passed_pawn[8];
	extern score backward_pawn[2][8];

	extern score mobility_knight[9];
	extern score mobility_bishop[14];
	extern score mobility_rook[15];
	extern score mobility_queen[7+7+7+6+1];

	extern score king_attack[200];

	extern short insufficient_material_threshold;

	extern score advanced_passed_pawn[8][6];

	void init();
	void update_derived();
	bool sane();
	bool normalize();
}

extern score pst[2][7][64];
void init_pst();
void update_pst();

#endif
