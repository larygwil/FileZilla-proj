#ifndef __EVAL_H__
#define __EVAL_H__

#include "chess.hpp"

#define LAZY_EVAL 756

struct eval_values_t
{
	eval_values_t();

	short material_values[7];

	// Derived
	short initial_material;

	short double_bishop;

	short doubled_pawn[2];
	short passed_pawn[2];
	short isolated_pawn[2];
	short connected_pawn[2];
	
	short pawn_shield[2];

	short castled;

	short pin_absolute_bishop;
	short pin_absolute_rook;
	short pin_absolute_queen;

	short mobility_scale[2];

	short pin_scale[2];

	short rooks_on_open_file_scale;

	short connected_rooks_scale[2];

	short tropism_scale[2];

	short king_attack_by_piece[6];
	short king_check_by_piece[6];
	short king_melee_attack_by_rook;
	short king_melee_attack_by_queen;

	short king_attack_min[2];
	short king_attack_max[2];
	short king_attack_rise[2];
	short king_attack_exponent[2];
	short king_attack_offset[2];

	short center_control_scale[2];

	short phase_transition_begin;
	short phase_transition_duration;

	short material_imbalance_scale[2];

	short rule_of_the_square;
	short passed_pawn_unhindered;

	short unstoppable_pawn_scale[2];

	short hanging_piece[6];
	short hanging_piece_scale[2];

	short mobility_knight_min;
	short mobility_knight_max;
	short mobility_knight_rise;
	short mobility_knight_offset;
	short mobility_bishop_min;
	short mobility_bishop_max;
	short mobility_bishop_rise;
	short mobility_bishop_offset;
	short mobility_rook_min;
	short mobility_rook_max;
	short mobility_rook_rise;
	short mobility_rook_offset;
	short mobility_queen_min;
	short mobility_queen_max;
	short mobility_queen_rise;
	short mobility_queen_offset;

	// Derived
	int phase_transition_material_begin;
	int phase_transition_material_end;

	short mobility_knight[8];
	short mobility_bishop[13];
	short mobility_rook[14];
	short mobility_queen[7+7+7+6];

	short king_attack[2][150];

	void update_derived();
};

extern eval_values_t eval_values;

short evaluate_fast( position const& p, color::type c );

short evaluate_full( position const& p, color::type c );
short evaluate_full( position const& p, color::type c, short eval_fast );

short evaluate_move( position const& p, color::type c, short current_evaluation, move const& m, position::pawn_structure& outPawns );

void evaluate_pawns( uint64_t const white_pawns, uint64_t const black_pawns, short* eval, uint64_t& passed );

short get_material_value( pieces::type pi );

short phase_scale( short const* material, short ev1, short ev2 );

// Piece-square tables
extern short const pawn_values[2][64];
extern short const knight_values[2][64];
extern short const bishop_values[2][64];
extern short const rook_values[2][64];
extern short const queen_values[2][64];

#endif
