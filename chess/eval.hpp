#ifndef __EVAL_H__
#define __EVAL_H__

#include "chess.hpp"

struct eval_values_t
{
	eval_values_t();

	short material_values[7];
	short initial_material;

	short double_bishop;
	short doubled_pawn;
	short passed_pawn;
	short isolated_pawn;
	short connected_pawn;
	short pawn_shield;
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

	void update_derived();
};

extern eval_values_t eval_values;

short evaluate_fast( position const& p, color::type c );

short evaluate_full( position const& p, color::type c );
short evaluate_full( position const& p, color::type c, short eval_fast );

short evaluate_move( position const& p, color::type c, short current_evaluation, move const& m, position::pawn_structure& outPawns );

short evaluate_pawns( unsigned long long const white_pawns, unsigned long long const black_pawns );

short get_material_value( pieces::type pi );

#endif
