#ifndef __EVAL_H__
#define __EVAL_H__

#include "chess.hpp"
#include "score.hpp"

#define LAZY_EVAL 756

struct eval_values_t
{
	eval_values_t();

	short mg_material_values[7];
	short eg_material_values[7];

	score double_bishop;

	score doubled_pawn;
	score passed_pawn;
	score isolated_pawn;
	score connected_pawn;
	
	score pawn_shield;

	short castled;

	score pin_absolute_bishop;
	score pin_absolute_rook;
	score pin_absolute_queen;

	score rooks_on_open_file;
	score rooks_on_half_open_file;

	score connected_rooks;

	score tropism;

	short king_attack_by_piece[6];
	short king_check_by_piece[6];
	short king_melee_attack_by_rook;
	short king_melee_attack_by_queen;

	short king_attack_min[2];
	short king_attack_max[2];
	short king_attack_rise[2];
	short king_attack_exponent[2];
	short king_attack_offset[2];

	score center_control;

	short phase_transition_begin;
	short phase_transition_duration;

	score material_imbalance;

	score rule_of_the_square;
	score passed_pawn_unhindered;

	score hanging_piece[6];

	score mobility_knight_min;
	score mobility_knight_max;
	score mobility_knight_rise;
	score mobility_knight_offset;
	score mobility_bishop_min;
	score mobility_bishop_max;
	score mobility_bishop_rise;
	score mobility_bishop_offset;
	score mobility_rook_min;
	score mobility_rook_max;
	score mobility_rook_rise;
	score mobility_rook_offset;
	score mobility_queen_min;
	score mobility_queen_max;
	score mobility_queen_rise;
	score mobility_queen_offset;

	score side_to_move;

	// Derived
	score material_values[7];

	score initial_material;

	int phase_transition_material_begin;
	int phase_transition_material_end;

	score mobility_knight[8];
	score mobility_bishop[13];
	score mobility_rook[14];
	score mobility_queen[7+7+7+6];

	score king_attack[150];

	void update_derived();
};

extern eval_values_t eval_values;

short evaluate_full( position const& p, color::type c );
short evaluate_full( position const& p, color::type c );

short evaluate_move( position const& p, color::type c, move const& m );

score evaluate_pawns( uint64_t const white_pawns, uint64_t const black_pawns, uint64_t& passed );

score get_material_value( pieces::type pi );

void init_pst();
extern score pst[2][7][64];


#endif
