#include "chess.hpp"
#include "eval_values.hpp"

#include <cmath>

namespace eval_values {

score material_values[7];

score double_bishop;

score doubled_pawn[2][8];
score passed_pawn[8];
score passed_pawn_advance_power;
score isolated_pawn[2][8];
score connected_pawn[2][8];
score candidate_passed_pawn[8];
score backward_pawn[2][8];
score passed_pawn_king_distance[2];

score pawn_shield[3];

short castled;

score absolute_pin[7];

score rooks_on_open_file;
score rooks_on_half_open_file;

score connected_rooks;

score tropism[7];

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

score attacked_piece[6];
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

short drawishness;

score rooks_on_rank_7;

score knight_outposts[2];
score bishop_outposts[2];

score initial_material;

int phase_transition_material_begin;
int phase_transition_material_end;

score mobility_knight[9];
score mobility_bishop[14];
score mobility_rook[15];
score mobility_queen[7+7+7+6+1];

score king_attack[200];

short insufficient_material_threshold;

score advanced_passed_pawn[8][6];

score trapped_rook[2];

void init()
{
	// Untweaked values
	material_values[pieces::king] = score( 20000, 20000 );
	king_attack_by_piece[pieces::none] =     0;
	king_check_by_piece[pieces::none]  =     0;
	king_check_by_piece[pieces::pawn]  =     0;

	// Tweaked values
	material_values[pieces::pawn]   = score( 72, 95 );
	material_values[pieces::knight] = score( 357, 323 );
	material_values[pieces::bishop] = score( 328, 322 );
	material_values[pieces::rook]   = score( 447, 595 );
	material_values[pieces::queen]  = score( 1150, 1084 );
	double_bishop                   = score( 21, 67 );
	doubled_pawn[0][0]              = score( -4, -26 );
	doubled_pawn[0][1]              = score( 0, -13 );
	doubled_pawn[0][2]              = score( -1, -25 );
	doubled_pawn[0][3]              = score( -1, -2 );
	doubled_pawn[1][0]              = score( -14, -22 );
	doubled_pawn[1][1]              = score( -12, -20 );
	doubled_pawn[1][2]              = score( -22, -2 );
	doubled_pawn[1][3]              = score( -8, -18 );
	passed_pawn[0]                  = score( 9, 8 );
	passed_pawn[1]                  = score( 10, 9 );
	passed_pawn[2]                  = score( 10, 9 );
	passed_pawn[3]                  = score( 13, 8 );
	backward_pawn[0][0]             = score( -1, 0 );
	backward_pawn[0][1]             = score( -19, -3 );
	backward_pawn[0][2]             = score( -11, 0 );
	backward_pawn[0][3]             = score( -5, -18 );
	backward_pawn[1][0]             = score( -1, 0 );
	backward_pawn[1][1]             = score( -2, -5 );
	backward_pawn[1][2]             = score( -4, 0 );
	backward_pawn[1][3]             = score( -9, 0 );
	passed_pawn_advance_power       = score( 134, 183 );
	passed_pawn_king_distance[0]    = score( 0, 4 );
	passed_pawn_king_distance[1]    = score( 1, 4 );
	isolated_pawn[0][0]             = score( -23, -12 );
	isolated_pawn[0][1]             = score( -35, -19 );
	isolated_pawn[0][2]             = score( -18, -22 );
	isolated_pawn[0][3]             = score( -28, -15 );
	isolated_pawn[1][0]             = score( -6, -16 );
	isolated_pawn[1][1]             = score( -11, -14 );
	isolated_pawn[1][2]             = score( -8, -12 );
	isolated_pawn[1][3]             = score( -11, -12 );
	connected_pawn[0][0]            = score( 0, 0 );
	connected_pawn[0][1]            = score( 0, 3 );
	connected_pawn[0][2]            = score( 1, 2 );
	connected_pawn[0][3]            = score( 4, 3 );
	connected_pawn[1][0]            = score( 6, 0 );
	connected_pawn[1][1]            = score( 5, 0 );
	connected_pawn[1][2]            = score( 4, 5 );
	connected_pawn[1][3]            = score( 2, 10 );
	candidate_passed_pawn[0]        = score( 0, 19 );
	candidate_passed_pawn[1]        = score( 11, 3 );
	candidate_passed_pawn[2]        = score( 5, 20 );
	candidate_passed_pawn[3]        = score( 19, 14 );
	pawn_shield[0]                  = score( 31, 0 );
	pawn_shield[1]                  = score( 18, 3 );
	pawn_shield[2]                  = score( 11, 0 );
	absolute_pin[1]                 = score( 0, 6 );
	absolute_pin[2]                 = score( 0, 31 );
	absolute_pin[3]                 = score( 0, 7 );
	absolute_pin[4]                 = score( 0, 0 );
	absolute_pin[5]                 = score( 5, 1 );
	rooks_on_open_file              = score( 29, 12 );
	rooks_on_half_open_file         = score( 13, 5 );
	connected_rooks                 = score( 1, 2 );
	tropism[1]                      = score( 1, 0 );
	tropism[2]                      = score( 2, 1 );
	tropism[3]                      = score( 2, 0 );
	tropism[4]                      = score( 2, 0 );
	tropism[5]                      = score( 0, 4 );
	king_attack_by_piece[1]         = 3;
	king_attack_by_piece[2]         = 9;
	king_attack_by_piece[3]         = 4;
	king_attack_by_piece[4]         = 10;
	king_attack_by_piece[5]         = 70;
	king_check_by_piece[2]          = 0;
	king_check_by_piece[3]          = 10;
	king_check_by_piece[4]          = 4;
	king_check_by_piece[5]          = 2;
	king_melee_attack_by_rook       = 7;
	king_melee_attack_by_queen      = 9;
	king_attack_min[0]              = 50;
	king_attack_min[1]              = 32;
	king_attack_max[0]              = 1244;
	king_attack_max[1]              = 486;
	king_attack_rise[0]             = 2;
	king_attack_rise[1]             = 1;
	king_attack_exponent[0]         = 121;
	king_attack_exponent[1]         = 101;
	king_attack_offset[0]           = 82;
	king_attack_offset[1]           = 87;
	center_control                  = score( 4, 0 );
	material_imbalance              = score( 32, 25 );
	rule_of_the_square              = score( 2, 8 );
	passed_pawn_unhindered          = score( 12, 6 );
	attacked_piece[1]               = score( 3, 7 );
	attacked_piece[2]               = score( 0, 15 );
	attacked_piece[3]               = score( 9, 9 );
	attacked_piece[4]               = score( 9, 15 );
	attacked_piece[5]               = score( 22, 3 );
	hanging_piece[1]                = score( 3, 9 );
	hanging_piece[2]                = score( 14, 1 );
	hanging_piece[3]                = score( 6, 23 );
	hanging_piece[4]                = score( 7, 22 );
	hanging_piece[5]                = score( 9, 34 );
	mobility_knight_min             = score( -53, -14 );
	mobility_knight_max             = score( 62, 13 );
	mobility_knight_rise            = score( 6, 7 );
	mobility_knight_offset          = score( 1, 0 );
	mobility_bishop_min             = score( -19, -27 );
	mobility_bishop_max             = score( 20, 41 );
	mobility_bishop_rise            = score( 7, 9 );
	mobility_bishop_offset          = score( 0, 0 );
	mobility_rook_min               = score( -3, -42 );
	mobility_rook_max               = score( 9, 6 );
	mobility_rook_rise              = score( 2, 10 );
	mobility_rook_offset            = score( 0, 5 );
	mobility_queen_min              = score( -94, -36 );
	mobility_queen_max              = score( 15, 15 );
	mobility_queen_rise             = score( 1, 50 );
	mobility_queen_offset           = score( 0, 0 );
	side_to_move                    = score( 5, 0 );
	drawishness                     = -50;
	rooks_on_rank_7                 = score( 3, 49 );
	knight_outposts[0]              = score( 3, 5 );
	knight_outposts[1]              = score( 10, 21 );
	bishop_outposts[0]              = score( 0, 3 );
	bishop_outposts[1]              = score( 8, 13 );
	trapped_rook[0].mg()            = -26;
	trapped_rook[1].mg()            = -57;

	update_derived();
}

void update_derived()
{
	initial_material =
		material_values[pieces::pawn] * 8 +
		material_values[pieces::knight] * 2 +
		material_values[pieces::bishop] * 2 +
		material_values[pieces::rook] * 2 +
		material_values[pieces::queen];

	//phase_transition_material_begin = initial_material.mg() * 2 - phase_transition_begin;
	//phase_transition_material_end = phase_transition_material_begin - phase_transition_duration;
	phase_transition_material_begin = initial_material.mg() * 2;
	phase_transition_material_end = 1000;
	phase_transition_begin = 0;
	phase_transition_duration = phase_transition_material_begin - phase_transition_material_end;

	for( short i = 0; i <= 8; ++i ) {
		if( i > mobility_knight_offset.mg() ) {
			mobility_knight[i].mg() = (std::min)(short(mobility_knight_min.mg() + mobility_knight_rise.mg() * (i - mobility_knight_offset.mg())), mobility_knight_max.mg());
		}
		else {
			mobility_knight[i].mg() = mobility_knight_min.mg();
		}
		if( i > mobility_knight_offset.eg() ) {
			mobility_knight[i].eg() = (std::min)(short(mobility_knight_min.eg() + mobility_knight_rise.eg() * (i - mobility_knight_offset.eg())), mobility_knight_max.eg());
		}
		else {
			mobility_knight[i].eg() = mobility_knight_min.eg();
		}
	}

	for( short i = 0; i <= 13; ++i ) {
		if( i > mobility_bishop_offset.mg() ) {
			mobility_bishop[i].mg() = (std::min)(short(mobility_bishop_min.mg() + mobility_bishop_rise.mg() * (i - mobility_bishop_offset.mg())), mobility_bishop_max.mg());
		}
		else {
			mobility_bishop[i].mg() = mobility_bishop_min.mg();
		}
		if( i > mobility_bishop_offset.eg() ) {
			mobility_bishop[i].eg() = (std::min)(short(mobility_bishop_min.eg() + mobility_bishop_rise.eg() * (i - mobility_bishop_offset.eg())), mobility_bishop_max.eg());
		}
		else {
			mobility_bishop[i].eg() = mobility_bishop_min.eg();
		}
	}

	for( short i = 0; i <= 14; ++i ) {
		if( i > mobility_rook_offset.mg() ) {
			mobility_rook[i].mg() = (std::min)(short(mobility_rook_min.mg() + mobility_rook_rise.mg() * (i - mobility_rook_offset.mg())), mobility_rook_max.mg());
		}
		else {
			mobility_rook[i].mg() = mobility_rook_min.mg();
		}
		if( i > mobility_rook_offset.eg() ) {
			mobility_rook[i].eg() = (std::min)(short(mobility_rook_min.eg() + mobility_rook_rise.eg() * (i - mobility_rook_offset.eg())), mobility_rook_max.eg());
		}
		else {
			mobility_rook[i].eg() = mobility_rook_min.eg();
		}
	}

	for( short i = 0; i <= 27; ++i ) {
		if( i > mobility_queen_offset.mg() ) {
			mobility_queen[i].mg() = (std::min)(short(mobility_queen_min.mg() + mobility_queen_rise.mg() * (i - mobility_queen_offset.mg())), mobility_queen_max.mg());
		}
		else {
			mobility_queen[i].mg() = mobility_queen_min.mg();
		}
		if( i > mobility_queen_offset.eg() ) {
			mobility_queen[i].eg() = (std::min)(short(mobility_queen_min.eg() + mobility_queen_rise.eg() * (i - mobility_queen_offset.eg())), mobility_queen_max.eg());
		}
		else {
			mobility_queen[i].eg() = mobility_queen_min.eg();
		}
	}

	for( short i = 0; i < 200; ++i ) {
		short v[2];
		for( int c = 0; c < 2; ++c ) {
			if( i > king_attack_offset[c] ) {
				double factor = i - king_attack_offset[c];
				factor = std::pow( factor, double(king_attack_exponent[c]) / 100.0 );
				v[c] = (std::min)(short(king_attack_min[c] + king_attack_rise[c] * factor), short(king_attack_min[c] + king_attack_max[c]));
			}
			else {
				v[c] = 0;
			}
		}
		king_attack[i] = score( v[0], v[1] );
	}

	insufficient_material_threshold = (std::max)(
			material_values[pieces::knight].eg(),
			material_values[pieces::bishop].eg()
		);

	for( int file = 0; file < 4; ++file ) {
		passed_pawn[7 - file] = passed_pawn[file];
		candidate_passed_pawn[7 - file] = candidate_passed_pawn[file];
		for( int i = 0; i < 2; ++i ) {
			doubled_pawn[i][7 - file] = doubled_pawn[i][file];
			isolated_pawn[i][7 - file] = isolated_pawn[i][file];
			connected_pawn[i][7 - file] = connected_pawn[i][file];
			backward_pawn[i][7 - file] = backward_pawn[i][file];
		}
	}

	for( int file = 0; file < 8; ++file ) {
		for( int i = 0; i < 6; ++i ) {
			advanced_passed_pawn[file][i].mg() = double(passed_pawn[file].mg()) * (1 + std::pow( double(i), double(passed_pawn_advance_power.mg()) / 100.0 ) );
			advanced_passed_pawn[file][i].eg() = double(passed_pawn[file].eg()) * (1 + std::pow( double(i), double(passed_pawn_advance_power.eg()) / 100.0 ) );
		}
	}
}
}


namespace pst_data {
typedef score s;

// All piece-square-tables are from white's point of view, files a-d. Files e-h and black view are derived symmetrially.
// Middle-game:
// - Rook pawns are worth less, center pawns are worth more.
// - Some advancement is good, too far advancement isn't
// - In end-game, pawns are equal. Strength comes from interaction with other pieces
const score pawn_values[32] = {
	s(  0, 0), s( 0,0), s( 0,0), s( 0,0),
	s(-15, 0), s(-5,0), s(-5,0), s(-5,0),
	s(-13, 0), s(-2,0), s( 0,0), s( 0,0),
	s(-10, 0), s( 0,0), s( 5,0), s(10,0),
	s(-10 ,0), s(-3,0), s( 3,0), s( 5,0),
	s(-10, 0), s(-5,0), s(-2,0), s( 0,0),
	s(-10, 0), s(-5,0), s(-2,0), s( 0,0),
	s( -5, 0), s(-5,0), s(-5,0), s(-5,0)
};

// Corners = bad, edges = bad. Center = good.
// Middle game files a and h aren't as bad as ranks 1 and 8.
// End-game evaluation is less pronounced
const score knight_values[32] = {
	s(-50,-40), s(-43,-34), s(-36,-27), s(-30,-20),
	s(-40,-34), s(-25,-27), s(-15,-15), s(-10, -5),
	s(-20,-27), s(-10,-15), s(  0, -2), s(  5,  2),
	s(-10,-20), s(  0, -5), s( 10,  2), s( 15,  5),
	s( -5,-20), s(  5, -5), s( 15,  2), s( 20,  5),
	s( -5,-27), s(  5,-15), s( 15, -2), s( 20,  2),
	s(-40,-34), s(-10,-27), s(  0,-15), s(  5, -5),
	s(-55,-40), s(-20,-35), s(-15,-27), s(-10,-20)
};

// Just like knights. Slightly less mg values for pieces not on the
// rays natural to a bishop from its starting position.
const score bishop_values[32] = {
	s(-15,-25), s(-15,-15), s(-13,-12), s( -8,-10),
	s( -7,-15), s(  0,-10), s( -2, -7), s(  0, -5),
	s( -5,-12), s( -2, -7), s(  3, -5), s(  2, -2),
	s( -3,-10), s(  0, -5), s(  2, -4), s(  7,  2),
	s( -3,-10), s(  0, -5), s(  2, -4), s(  7,  2),
	s( -5,-12), s( -2, -7), s(  3, -5), s(  2, -2),
	s( -7,-15), s(  0,-10), s( -2, -7), s(  0, -5),
	s(- 7,-25), s( -7,-15), s( -5,-12), s( -3,-10)
};

// Center files are more valuable, precisely due to the central control exercised by it.
// Very slightly prefer home row and 7th rank
// In endgame, it doesn't matter much where the rook is if you disregard other pieces.
const score rook_values[32] = {
	s(-3,0), s(-1,0), s(1,0), s(3,0),
	s(-4,0), s(-2,0), s(0,0), s(2,0),
	s(-4,0), s(-2,0), s(0,0), s(2,0),
	s(-4,0), s(-2,0), s(0,0), s(2,0),
	s(-4,0), s(-2,0), s(0,0), s(2,0),
	s(-4,0), s(-2,0), s(0,0), s(2,0),
	s(-3,0), s(-1,0), s(1,0), s(3,0),
	s(-4,0), s(-2,0), s(0,0), s(2,0)
};

// The queen is so versatile and mobile, it does not really matter where
// she is in middle-game. End-game prefer center.
const score queen_values[32] = {
	s(-1,-30), s(0,-20), s(0,-10), s(0,-5),
	s(0, -20), s(0,-10), s(0, -5), s(0, 0),
	s(0, -10), s(0, -5), s(0,  0), s(1, 0),
	s(0,  -5), s(0,  0), s(1,  0), s(1, 5),
	s(0,  -5), s(0,  0), s(1,  0), s(1, 5),
	s(0, -10), s(0, -5), s(0,  0), s(1, 0),
	s(0, -20), s(0,-10), s(0, -5), s(0, 0),
	s(-1,-30), s(0,-20), s(0,-10), s(0,-5)
};

// Middle-game, prefer shelter on home row behind own pawns.
// End-game, keep an open mind in the center.
const score king_values[32] = {
	s( 45,-55), s( 50,-35), s( 40,-15), s( 20, -5),
	s( 40,-35), s( 45,-15), s( 25,  0), s( 15, 10),
	s( 20,-15), s( 25,  0), s( 15, 10), s( -5, 30),
	s( 15, -5), s( 20, 10), s(  0, 30), s(-15, 50),
	s(  0, -5), s( 15, 10), s( -5, 30), s(-20, 50),
	s( -5,-15), s(  0,  0), s(-15, 10), s(-30, 30),
	s(-15,-35), s( -5,-15), s(-20,  0), s(-35, 10),
	s(-20,-55), s(-15,-35), s(-30,-15), s(-40, -5)
};
}


score pst[2][7][64];


void init_pst()
{
	for( int i = 0; i < 32; ++i ) {
		pst[0][1][(i / 4) * 8 + i % 4] = pst_data::pawn_values[i];
		pst[0][2][(i / 4) * 8 + i % 4] = pst_data::knight_values[i];
		pst[0][3][(i / 4) * 8 + i % 4] = pst_data::bishop_values[i];
		pst[0][4][(i / 4) * 8 + i % 4] = pst_data::rook_values[i];
		pst[0][5][(i / 4) * 8 + i % 4] = pst_data::queen_values[i];
		pst[0][6][(i / 4) * 8 + i % 4] = pst_data::king_values[i];
		pst[0][1][(i / 4) * 8 + 7 - i % 4] = pst_data::pawn_values[i];
		pst[0][2][(i / 4) * 8 + 7 - i % 4] = pst_data::knight_values[i];
		pst[0][3][(i / 4) * 8 + 7 - i % 4] = pst_data::bishop_values[i];
		pst[0][4][(i / 4) * 8 + 7 - i % 4] = pst_data::rook_values[i];
		pst[0][5][(i / 4) * 8 + 7 - i % 4] = pst_data::queen_values[i];
		pst[0][6][(i / 4) * 8 + 7 - i % 4] = pst_data::king_values[i];
	}

	for( int i = 0; i < 64; ++i ) {
		for( int p = 1; p < 7; ++p ) {
			pst[1][p][i] = pst[0][p][63-i];
		}
	}
}
