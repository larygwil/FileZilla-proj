#ifndef __EVAL_H__
#define __EVAL_H__

#include "chess.hpp"

#define LAZY_EVAL 529

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

	// Try to avoid "slow" floating point arithmetic, or at least the conversion between int and float/double.
	// mobility = raw_data * multiplicator / divisor.
	short mobility_multiplicator;
	short mobility_divisor;

	short pin_multiplicator;
	short pin_divisor;

	short rooks_on_open_file_multiplicator;
	short rooks_on_open_file_divisor;

	short tropism_multiplicator;
	short tropism_divisor;

	short king_attack_multiplicator;
	short king_attack_divisor;

	short center_control_multiplicator;
	short center_control_divisor;

	short phase_transition_begin;
	short phase_transition_duration;

	// Derived
	int phase_transition_material_begin;
	int phase_transition_material_end;

	void update_derived();
};

extern eval_values_t eval_values;

short evaluate_fast( position const& p, color::type c );

short evaluate_full( position const& p, color::type c );
short evaluate_full( position const& p, color::type c, short eval_fast );

short evaluate_move( position const& p, color::type c, short current_evaluation, move const& m, position::pawn_structure& outPawns );

void evaluate_pawns( uint64_t const white_pawns, uint64_t const black_pawns, short* eval );

short get_material_value( pieces::type pi );

short phase_scale( short const* material, short ev1, short ev2 );

#endif
