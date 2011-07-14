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


int evaluate_side( color::type c, position const& p )
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
		piece const& pp = p.pieces[c][pieces::knight1 + 1];
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

int evaluate( color::type c, position const& p )
{
	int value = evaluate_side( c, p ) - evaluate_side( static_cast<color::type>(1-c), p );

	ASSERT( value > result::loss && value < result::win );

	return value;
}
