#include "chess.hpp"
#include "eval_values.hpp"

#include <cmath>

namespace eval_values {

short mg_material_values[7];
short eg_material_values[7];

score double_bishop;

score doubled_pawn;
score passed_pawn;
score isolated_pawn;
score connected_pawn;
score candidate_passed_pawn;

score pawn_shield;

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

// Derived
score material_values[7];

score initial_material;

int phase_transition_material_begin;
int phase_transition_material_end;

score mobility_knight[9];
score mobility_bishop[14];
score mobility_rook[15];
score mobility_queen[7+7+7+6+1];

score king_attack[200];

short insufficient_material_threshold;

void init()
{
	// Untweaked values
	mg_material_values[pieces::none]   =     0;
	mg_material_values[pieces::king]   = 20000;
	eg_material_values[pieces::none]   =     0;
	eg_material_values[pieces::king]   = 20000;
	king_attack_by_piece[pieces::none] =     0;
	king_check_by_piece[pieces::none]  =     0;
	king_check_by_piece[pieces::pawn]  =     0;

	// Tweaked values
	mg_material_values[1]          =    73;
	mg_material_values[2]          =   360;
	mg_material_values[3]          =   390;
	mg_material_values[4]          =   504;
	mg_material_values[5]          =  1187;
	eg_material_values[1]          =   105;
	eg_material_values[2]          =   320;
	eg_material_values[3]          =   364;
	eg_material_values[4]          =   563;
	eg_material_values[5]          =  1027;
	double_bishop.mg()             =    38;
	double_bishop.eg()             =    37;
	doubled_pawn.mg()              =   -10;
	doubled_pawn.eg()              =   -13;
	passed_pawn.mg()               =    20;
	passed_pawn.eg()               =    18;
	isolated_pawn.mg()             =    -9;
	isolated_pawn.eg()             =   -15;
	connected_pawn.mg()            =     6;
	connected_pawn.eg()            =     0;
	candidate_passed_pawn.mg()     =    16;
	candidate_passed_pawn.eg()     =     6;
	pawn_shield.mg()               =    19;
	pawn_shield.eg()               =     0;
	absolute_pin[1].mg()           =     1;
	absolute_pin[1].eg()           =     1;
	absolute_pin[2].mg()           =     0;
	absolute_pin[2].eg()           =    55;
	absolute_pin[3].mg()           =     0;
	absolute_pin[3].eg()           =     1;
	absolute_pin[4].mg()           =     1;
	absolute_pin[4].eg()           =     0;
	absolute_pin[5].mg()           =     6;
	absolute_pin[5].eg()           =    19;
	rooks_on_open_file.mg()        =    20;
	rooks_on_open_file.eg()        =    20;
	rooks_on_half_open_file.mg()   =     9;
	rooks_on_half_open_file.eg()   =    11;
	connected_rooks.mg()           =     0;
	connected_rooks.eg()           =    21;
	tropism[1].mg()                =     2;
	tropism[1].eg()                =     0;
	tropism[2].mg()                =     3;
	tropism[2].eg()                =     0;
	tropism[3].mg()                =     1;
	tropism[3].eg()                =     0;
	tropism[4].mg()                =     1;
	tropism[4].eg()                =     0;
	tropism[5].mg()                =     1;
	tropism[5].eg()                =     2;
	king_attack_by_piece[1]        =     1;
	king_attack_by_piece[2]        =     8;
	king_attack_by_piece[3]        =     7;
	king_attack_by_piece[4]        =    12;
	king_attack_by_piece[5]        =    19;
	king_check_by_piece[2]         =    11;
	king_check_by_piece[3]         =     9;
	king_check_by_piece[4]         =     1;
	king_check_by_piece[5]         =     5;
	king_melee_attack_by_rook      =     2;
	king_melee_attack_by_queen     =    12;
	king_attack_min[0]             =    18;
	king_attack_max[0]             =   367;
	king_attack_rise[0]            =     2;
	king_attack_exponent[0]        =   133;
	king_attack_offset[0]          =    26;
	king_attack_min[1]             =     5;
	king_attack_max[1]             =   597;
	king_attack_rise[1]            =     2;
	king_attack_exponent[1]        =   137;
	king_attack_offset[1]          =    95;
	center_control.mg()            =     0;
	center_control.eg()            =     0;
	material_imbalance.mg()        =     9;
	material_imbalance.eg()        =    18;
	rule_of_the_square.mg()        =    15;
	rule_of_the_square.eg()        =    21;
	passed_pawn_unhindered.mg()    =     1;
	passed_pawn_unhindered.eg()    =    17;
	hanging_piece[1].mg()          =     4;
	hanging_piece[1].eg()          =     8;
	hanging_piece[2].mg()          =     8;
	hanging_piece[2].eg()          =    12;
	hanging_piece[3].mg()          =     9;
	hanging_piece[3].eg()          =    13;
	hanging_piece[4].mg()          =    21;
	hanging_piece[4].eg()          =     3;
	hanging_piece[5].mg()          =    19;
	hanging_piece[5].eg()          =     8;
	mobility_knight_min.mg()       =   -19;
	mobility_knight_max.mg()       =     3;
	mobility_knight_rise.mg()      =     6;
	mobility_knight_offset.mg()    =     1;
	mobility_bishop_min.mg()       =   -49;
	mobility_bishop_max.mg()       =    66;
	mobility_bishop_rise.mg()      =     4;
	mobility_bishop_offset.mg()    =     0;
	mobility_rook_min.mg()         =   -13;
	mobility_rook_max.mg()         =    33;
	mobility_rook_rise.mg()        =     2;
	mobility_rook_offset.mg()      =     3;
	mobility_queen_min.mg()        =   -66;
	mobility_queen_max.mg()        =    36;
	mobility_queen_rise.mg()       =     2;
	mobility_queen_offset.mg()     =     0;
	mobility_knight_min.eg()       =   -19;
	mobility_knight_max.eg()       =    34;
	mobility_knight_rise.eg()      =     8;
	mobility_knight_offset.eg()    =     0;
	mobility_bishop_min.eg()       =   -21;
	mobility_bishop_max.eg()       =    19;
	mobility_bishop_rise.eg()      =     7;
	mobility_bishop_offset.eg()    =     2;
	mobility_rook_min.eg()         =   -28;
	mobility_rook_max.eg()         =    36;
	mobility_rook_rise.eg()        =    10;
	mobility_rook_offset.eg()      =     3;
	mobility_queen_min.eg()        =   -30;
	mobility_queen_max.eg()        =    92;
	mobility_queen_rise.eg()       =    36;
	mobility_queen_offset.eg()     =     0;
	side_to_move.mg()              =     3;
	side_to_move.eg()              =     0;
	drawishness                    =   -56;

	update_derived();
}

void update_derived()
{
	for( unsigned int i = 0; i < 7; ++i ) {
		material_values[i] = score( mg_material_values[i], eg_material_values[i] );
	}

	initial_material =
		material_values[pieces::pawn] * 8 +
		material_values[pieces::knight] * 2 +
		material_values[pieces::bishop] * 2 +
		material_values[pieces::rook] * 2 +
		material_values[pieces::queen];

	//phase_transition_material_begin = initial_material * 2; - phase_transition_begin;
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
			eg_material_values[pieces::knight],
			eg_material_values[pieces::bishop]
		);
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
