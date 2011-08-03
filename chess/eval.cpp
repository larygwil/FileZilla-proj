#include "eval.hpp"
#include "assert.hpp"

#include <iostream>

namespace special_values {
enum type
{
	double_bishop = 25,
	castled = 25
};
}

namespace {
unsigned char const pawn_values[2][8][8] =
	{
		{
			{ 0, 105, 105, 100,  97, 106, 120, 0 },
			{ 0, 110,  95, 100, 103, 112, 127, 0 },
			{ 0, 110,  90, 110, 117, 125, 135, 0 },
			{ 0,  80, 100, 120, 127, 140, 150, 0 },
			{ 0,  80, 100, 120, 127, 140, 150, 0 },
			{ 0, 110,  90, 110, 117, 125, 135, 0 },
			{ 0, 110,  95, 100, 103, 112, 127, 0 },
			{ 0, 105, 105, 100, 100, 106, 120, 0 }
		},
		{
			{ 0, 120, 106,  97, 100, 105, 105, 0 },
			{ 0, 127, 112, 103, 100,  95, 110, 0 },
			{ 0, 135, 125, 117, 110,  90, 110, 0 },
			{ 0, 150, 140, 127, 120, 100,  80, 0 },
			{ 0, 150, 140, 127, 120, 100,  80, 0 },
			{ 0, 135, 125, 117, 110,  90, 110, 0 },
			{ 0, 127, 112, 103, 100,  95, 110, 0 },
			{ 0, 120, 106, 100, 100, 105, 105, 0 }
		}

	};

signed short const queen_values[2][8][8] = {
	{
		{ 880, 890, 890, 895, 900, 890, 890, 880 },
		{ 890, 900, 905, 900, 900, 900, 900, 890 },
		{ 890, 905, 905, 905, 905, 905, 900, 890 },
		{ 895, 900, 905, 905, 905, 905, 900, 895 },
		{ 895, 900, 905, 905, 905, 905, 900, 895 },
		{ 890, 900, 905, 905, 905, 905, 900, 890 },
		{ 890, 900, 900, 900, 900, 900, 900, 890 },
		{ 880, 890, 890, 895, 895, 890, 890, 880 }
	},
	{
		{ 880, 890, 890, 895, 900, 890, 890, 880 },
		{ 890, 900, 900, 900, 900, 900, 900, 890 },
		{ 890, 900, 905, 905, 905, 905, 900, 890 },
		{ 895, 900, 905, 905, 905, 905, 900, 895 },
		{ 895, 900, 905, 905, 905, 905, 900, 895 },
		{ 890, 900, 905, 905, 905, 905, 905, 890 },
		{ 890, 900, 900, 900, 900, 905, 900, 890 },
		{ 880, 890, 890, 895, 895, 890, 890, 880 }
	}
};

signed short const rook_values[2][8][8] = {
	{
		{ 500, 495, 495, 495, 495, 495, 505, 500 },
		{ 500, 500, 500, 500, 500, 500, 510, 500 },
		{ 500, 500, 500, 500, 500, 500, 510, 500 },
		{ 505, 500, 500, 500, 500, 500, 510, 500 },
		{ 505, 500, 500, 500, 500, 500, 510, 500 },
		{ 500, 500, 500, 500, 500, 500, 510, 500 },
		{ 500, 500, 500, 500, 500, 500, 510, 500 },
		{ 500, 495, 495, 495, 495, 495, 505, 500 }
	},
	{
		{ 500, 505, 495, 495, 495, 495, 495, 500 },
		{ 500, 510, 500, 500, 500, 500, 500, 500 },
		{ 500, 510, 500, 500, 500, 500, 500, 500 },
		{ 500, 510, 500, 500, 500, 500, 500, 505 },
		{ 500, 510, 500, 500, 500, 500, 500, 505 },
		{ 500, 510, 500, 500, 500, 500, 500, 500 },
		{ 500, 510, 500, 500, 500, 500, 500, 500 },
		{ 500, 505, 495, 495, 495, 495, 495, 500 }
	}
};

signed short const knight_values[2][8][8] = {
	{
		{ 260, 270, 280, 280, 280, 280, 270, 260 },
		{ 270, 290, 305, 310, 305, 310, 290, 270 },
		{ 280, 310, 310, 315, 315, 310, 310, 280 },
		{ 280, 305, 315, 320, 320, 315, 310, 280 },
		{ 280, 305, 315, 320, 320, 315, 310, 280 },
		{ 280, 310, 310, 315, 315, 310, 310, 280 },
		{ 270, 290, 305, 310, 305, 310, 290, 270 },
		{ 260, 270, 280, 280, 280, 280, 270, 260 }
	},
	{
		{ 260, 270, 280, 280, 280, 280, 270, 260 },
		{ 270, 290, 310, 305, 310, 305, 290, 270 },
		{ 280, 310, 310, 315, 315, 310, 310, 280 },
		{ 280, 310, 315, 320, 320, 315, 305, 280 },
		{ 280, 310, 315, 320, 320, 315, 305, 280 },
		{ 280, 310, 310, 315, 315, 310, 310, 280 },
		{ 270, 290, 310, 305, 310, 305, 290, 270 },
		{ 260, 270, 280, 280, 280, 280, 270, 260 },
	}
};

signed short const bishop_values[2][8][8] = {
	{
		{ 300, 310, 310, 310, 310, 310, 310, 300 },
		{ 310, 325, 330, 320, 325, 320, 320, 310 },
		{ 310, 320, 330, 330, 325, 325, 320, 310 },
		{ 310, 320, 330, 330, 330, 330, 320, 310 },
		{ 310, 320, 330, 330, 330, 330, 320, 310 },
		{ 310, 320, 330, 330, 325, 325, 320, 310 },
		{ 310, 325, 330, 320, 325, 320, 320, 310 },
		{ 300, 310, 310, 310, 310, 310, 310, 300 }
	},
	{
		{ 300, 310, 310, 310, 310, 310, 310, 300 },
		{ 310, 320, 320, 325, 320, 330, 325, 310 },
		{ 310, 320, 325, 325, 330, 330, 320, 310 },
		{ 310, 320, 330, 330, 330, 330, 320, 310 },
		{ 310, 320, 330, 330, 330, 330, 320, 310 },
		{ 310, 320, 325, 325, 330, 330, 320, 310 },
		{ 310, 320, 320, 325, 320, 330, 325, 310 },
		{ 300, 310, 310, 310, 310, 310, 310, 300 }
	}
};

}


short evaluate_side( position const& p, color::type c )
{
	short result = 0;

	// To start: Count material in centipawns
	for( unsigned int i = 0; i < 16; ++i) {
		piece const& pp = p.pieces[c][i];
		if( pp.alive ) {
			switch( i ) {
			case pieces::pawn1:
			case pieces::pawn2:
			case pieces::pawn3:
			case pieces::pawn4:
			case pieces::pawn5:
			case pieces::pawn6:
			case pieces::pawn7:
			case pieces::pawn8:
				if (pp.special) {
					// Promoted pawn
					unsigned short promoted = (p.promotions[c] >> ((i - pieces::pawn1) * 2)) & 0x03;
					switch( promoted ) {
					case promotions::queen:
						result += queen_values[c][pp.column][pp.row];
						break;
					case promotions::rook:
						result += rook_values[c][pp.column][pp.row];
						break;
					case promotions::bishop:
						result += bishop_values[c][pp.column][pp.row];
						break;
					case promotions::knight:
						result += knight_values[c][pp.column][pp.row];
						break;
					}
				}
				else {
					result += pawn_values[c][pp.column][pp.row];
				}
				break;
			case pieces::king:
				break;
			case pieces::queen:
				result += queen_values[c][pp.column][pp.row];
				break;
			case pieces::rook1:
			case pieces::rook2:
				result += rook_values[c][pp.column][pp.row];
				break;
			case pieces::bishop1:
			case pieces::bishop2:
				result += bishop_values[c][pp.column][pp.row];
				break;
			case pieces::knight1:
			case pieces::knight2:
				result += knight_values[c][pp.column][pp.row];
				break;
			}
		}
	}

	if( p.pieces[c][pieces::bishop1].alive && p.pieces[c][pieces::bishop2].alive ) {
		result += special_values::double_bishop;
	}
	if( p.pieces[c][pieces::king].special ) {
		result += special_values::castled;
	}

	return result;
}

short evaluate( position const& p, color::type c )
{
	int value = evaluate_side( p, c ) - evaluate_side( p, static_cast<color::type>(1-c) );

	ASSERT( value > result::loss && value < result::win );

	return value;
}

namespace {
static short get_piece_value( position const& p, color::type c, int target, int col, int row )
{
	short eval = 0;
	switch( target ) {
	case pieces::pawn1:
	case pieces::pawn2:
	case pieces::pawn3:
	case pieces::pawn4:
	case pieces::pawn5:
	case pieces::pawn6:
	case pieces::pawn7:
	case pieces::pawn8:
	{
		piece const& pp = p.pieces[c][target];
		if( pp.special ) {
			unsigned short promoted = (p.promotions[c] >> ( 2 * (target - pieces::pawn1) ) ) & 0x03;
			switch( promoted ) {
			case promotions::queen:
				eval += queen_values[c][col][row];
				break;
			case promotions::rook:
				eval += rook_values[c][col][row];
				break;
			case promotions::bishop:
				eval += bishop_values[c][col][row];
				break;
			case promotions::knight:
				eval += knight_values[c][col][row];
				break;
			}
		}
		else {
			eval += pawn_values[c][col][row];
		}
		break;
	}
	case pieces::king:
		break;
	case pieces::queen:
		eval += queen_values[c][col][row];
		break;
	case pieces::rook1:
	case pieces::rook2:
		eval += rook_values[c][col][row];
		break;
	case pieces::bishop1:
	case pieces::bishop2:
		eval += bishop_values[c][col][row];
		break;
	case pieces::knight1:
	case pieces::knight2:
		eval += knight_values[c][col][row];
		break;
	}

	return eval;
}
}

short evaluate_move( position const& p, color::type c, short current_evaluation, move const& m )
{
	int target = p.board[m.target_col][m.target_row];
	if( target != pieces::nil ) {
		target &= 0x0f;
		current_evaluation += get_piece_value( p, static_cast<color::type>(1-c), target, m.target_col, m.target_row );
		if( target == pieces::bishop1 ) {
			if( p.pieces[1-c][pieces::bishop2].alive ) {
				current_evaluation += special_values::double_bishop;
			}
		}
		else if( target == pieces::bishop2 ) {
			if( p.pieces[1-c][pieces::bishop1].alive ) {
				current_evaluation += special_values::double_bishop;
			}
		}
	}

	int source = p.board[m.source_col][m.source_row] & 0x0f;

	if( source >= pieces::pawn1 && source <= pieces::pawn8 ) {
		piece const& pp = p.pieces[c][source];
		if( pp.special ) {
			current_evaluation -= get_piece_value( p, c, source, pp.column, pp.row );
			current_evaluation += get_piece_value( p, c, source, m.target_col, m.target_row );
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
				current_evaluation += queen_values[c][m.target_col][m.target_row];
			}
			else {
				current_evaluation -= pawn_values[c][pp.column][pp.row];
				current_evaluation += pawn_values[c][m.target_col][m.target_row];
			}
		}
	}
	else if( source == pieces::king ) {
		piece const& pp = p.pieces[c][source];
		if( pp.column == m.target_col + 2 ) {
			// Queenside
			current_evaluation += special_values::castled;
			current_evaluation -= get_piece_value( p, c, pieces::rook1, 0, pp.row );
			current_evaluation += get_piece_value( p, c, pieces::rook1, 3, m.target_row );
		}
		else if( pp.column + 2 == m.target_col ) {
			// Kingside
			current_evaluation += special_values::castled;
			current_evaluation -= get_piece_value( p, c, pieces::rook2, 7, pp.row );
			current_evaluation += get_piece_value( p, c, pieces::rook2, 5, m.target_row );
		}
	}
	else {
		piece const& pp = p.pieces[c][source];
		current_evaluation -= get_piece_value( p, c, source, pp.column, pp.row );
		current_evaluation += get_piece_value( p, c, source, m.target_col, m.target_row );
	}

	return current_evaluation;
}
