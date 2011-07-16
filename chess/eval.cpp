#include "eval.hpp"
#include "assert.hpp"

#include <iostream>

namespace {
short const piece_values[] = {
	100, 100, 100, 100, 100, 100, 100, 100,
	0, // Can't be captured
	900,
	500, 500,
	310, 310,
	300, 300
};

short const promotion_values[] = {
	900,
	500,
	310,
	300
};
}

namespace special_values {
enum type
{
	knight_at_border = 20,
	castled = 25
};
}

namespace {
unsigned char const pawn_values[2][8][8] =
	{
		{
			{ 0,  90,  90,  90,  97, 106, 117, 0 },
			{ 0,  95,  95,  95, 103, 112, 123, 0 },
			{ 0, 105, 105, 110, 117, 125, 133, 0 },
			{ 0, 110, 115, 120, 127, 140, 150, 0 },
			{ 0, 110, 115, 120, 127, 140, 150, 0 },
			{ 0, 105, 105, 110, 117, 125, 133, 0 },
			{ 0,  95,  95,  95, 103, 112, 123, 0 },
			{ 0,  90,  90,  90,  97, 106, 117, 0 }
		},
		{
			{ 0, 117, 106,  97,  90,  90,  90, 0 },
			{ 0, 123, 112, 103,  95,  95,  95, 0 },
			{ 0, 133, 125, 117, 110, 105, 105, 0 },
			{ 0, 150, 140, 127, 120, 115, 110, 0 },
			{ 0, 150, 140, 127, 120, 115, 110, 0 },
			{ 0, 133, 125, 117, 110, 105, 105, 0 },
			{ 0, 123, 112, 103,  95,  95,  95, 0 },
			{ 0, 117, 106,  97,  90,  90,  90, 0 }
		}

	};
}


int evaluate_side( position const& p, color::type c )
{
	unsigned int result = 0;

	// To start: Count material in centipawns
	for( unsigned int i = 0; i < 16; ++i) {
		piece const& pp = p.pieces[c][i];
		if( pp.alive ) {
			if( i >= pieces::pawn1 && i <= pieces::pawn8) {
				if (pp.special) {
					// Promoted pawn
					unsigned short promoted = (p.promotions[c] >> ((i - pieces::pawn1) * 2)) & 0x03;
					result += promotion_values[promoted];
				}
				else {
					result += pawn_values[c][pp.column][pp.row];
				}
			}
			else {
				result += piece_values[i];
			}
		}
	}

	for( int i = 0; i < 2; ++i ) {
		piece const& pp = p.pieces[c][pieces::knight1 + i];
		if( pp.alive ) {
			if( pp.column == 0 || pp.column == 7 ) {
				result -= special_values::knight_at_border;
			}

			if( pp.row == 0 || pp.row == 7 ) {
				result -= special_values::knight_at_border;
			}
		}
	}

	if( p.pieces[c][pieces::king].special ) {
		result += special_values::castled;
	}

	return result;
}

int evaluate( position const& p, color::type c )
{
	int value = evaluate_side( p, c ) - evaluate_side( p, static_cast<color::type>(1-c) );

	ASSERT( value > result::loss && value < result::win );

	return value;
}

namespace {
void subtract_target( position const& p, color::type c, int& eval, int target, int col, int row )
{
	if( target >= pieces::pawn1 && target <= pieces::pawn8 ) {
		piece const& pp = p.pieces[1-c][target];
		if( pp.special ) {
			unsigned short promoted = (p.promotions[1-c] >> ( 2 * (target - pieces::pawn1) ) ) & 0x03;
			// Not implemented in evaluate
//			if( promoted == promotions::knight ) {
//				if( !col || col == 7 ) {
//					eval -= special_values::knight_at_border;
//				}
//				if( !row || row == 7 ) {
//					eval -= special_values::knight_at_border;
//				}
//			}
			eval += promotion_values[ promoted ];
		}
		else {
			eval += pawn_values[1-c][col][row];
		}
	}
	else if( target == pieces::knight1 || target == pieces::knight2 ) {
		if( !col || col == 7 ) {
			eval -= special_values::knight_at_border;
		}
		if( !row || row == 7 ) {
			eval -= special_values::knight_at_border;
		}
		eval += piece_values[ target ];
	}
	else {
		eval += piece_values[ target ];
	}
}
}

int evaluate_move( position const& p, color::type c, int current_evaluation, move const& m )
{
	int target = p.board[m.target_col][m.target_row];
	if( target != pieces::nil ) {
		target &= 0x0f;
		subtract_target( p, c, current_evaluation, target, m.target_col, m.target_row );
	}

	if( m.piece >= pieces::pawn1 && m.piece <= pieces::pawn8 ) {
		piece const& pp = p.pieces[c][m.piece];
		if( pp.special ) {
			// Nothing changes
		}
		else {
			if( m.target_col != pp.column && target == pieces::nil ) {
				// Was en-passant
				current_evaluation += pawn_values[1-c][m.target_col][pp.row];
				current_evaluation -= pawn_values[c][pp.column][pp.row];
				current_evaluation += pawn_values[c][m.target_col][m.target_row];
			}
			else if( m.target_row == 0 || m.target_row == 7 ) {
				current_evaluation -= pawn_values[c][pp.column][pp.row];
				current_evaluation += promotion_values[promotions::queen];
			}
			else {
				current_evaluation -= pawn_values[c][pp.column][pp.row];
				current_evaluation += pawn_values[c][m.target_col][m.target_row];
			}
		}
	}
	else if( m.piece == pieces::knight1 || m.piece == pieces::knight2 ) {
		piece const& pp = p.pieces[c][m.piece];
		if( !m.target_col || m.target_col == 7 ) {
			current_evaluation -= special_values::knight_at_border;
		}
		else if( pp.column == 0 || pp.column == 7 ) {
			current_evaluation += special_values::knight_at_border;
		}
		if( !m.target_row || m.target_row == 7 ) {
			current_evaluation -= special_values::knight_at_border;
		}
		else if( pp.row == 0 || pp.row == 7 ) {
			current_evaluation += special_values::knight_at_border;
		}
	}
	else if( m.piece == pieces::king ) {
		piece const& pp = p.pieces[c][m.piece];
		if( (pp.column == m.target_col + 2) || (pp.column + 2 == m.target_col) ) {
			current_evaluation += special_values::castled;
		}
	}

	return current_evaluation;
}
