#include "chess.hpp"
#include "eval_values.hpp"

#include <cmath>

namespace eval_values {

short mg_material_values[7];
short eg_material_values[7];

score double_bishop;

score doubled_pawn;
score passed_pawn;
score passed_pawn_advance_power;
score isolated_pawn;
score connected_pawn;
score candidate_passed_pawn;
score backward_pawn;
score passed_pawn_king_distance[2];

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

score rooks_on_rank_7;

score knight_outposts[2];
score bishop_outposts[2];

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

score advanced_passed_pawn[6];

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
	mg_material_values[1]          =    75;
	mg_material_values[2]          =   264;
	mg_material_values[3]          =   309;
	mg_material_values[4]          =   488;
	mg_material_values[5]          =  1127;
	eg_material_values[1]          =    85;
	eg_material_values[2]          =   311;
	eg_material_values[3]          =   388;
	eg_material_values[4]          =   503;
	eg_material_values[5]          =  1013;
	double_bishop.mg()             =    42;
	double_bishop.eg()             =    33;
	doubled_pawn.mg()              =    -8;
	doubled_pawn.eg()              =    -6;
	passed_pawn.mg()               =     7;
	passed_pawn.eg()               =    10;
	backward_pawn.mg()             =    -4;
	backward_pawn.eg()             =     0;
	passed_pawn_advance_power.mg() =   200;
	passed_pawn_advance_power.eg() =   148;
	passed_pawn_king_distance[0].mg() =     0;
	passed_pawn_king_distance[0].eg() =     4;
	passed_pawn_king_distance[1].mg() =     3;
	passed_pawn_king_distance[1].eg() =     2;
	isolated_pawn.mg()             =    -9;
	isolated_pawn.eg()             =   -17;
	connected_pawn.mg()            =     4;
	connected_pawn.eg()            =     2;
	candidate_passed_pawn.mg()     =    14;
	candidate_passed_pawn.eg()     =    17;
	pawn_shield.mg()               =    20;
	pawn_shield.eg()               =     0;
	absolute_pin[1].mg()           =     0;
	absolute_pin[1].eg()           =    13;
	absolute_pin[2].mg()           =     4;
	absolute_pin[2].eg()           =    62;
	absolute_pin[3].mg()           =     1;
	absolute_pin[3].eg()           =     4;
	absolute_pin[4].mg()           =     0;
	absolute_pin[4].eg()           =     3;
	absolute_pin[5].mg()           =     1;
	absolute_pin[5].eg()           =     0;
	rooks_on_open_file.mg()        =    23;
	rooks_on_open_file.eg()        =    18;
	rooks_on_half_open_file.mg()   =     8;
	rooks_on_half_open_file.eg()   =    16;
	connected_rooks.mg()           =     2;
	connected_rooks.eg()           =     0;
	tropism[1].mg()                =     0;
	tropism[1].eg()                =     0;
	tropism[2].mg()                =     3;
	tropism[2].eg()                =     1;
	tropism[3].mg()                =     0;
	tropism[3].eg()                =     0;
	tropism[4].mg()                =     4;
	tropism[4].eg()                =     0;
	tropism[5].mg()                =     0;
	tropism[5].eg()                =     7;
	king_attack_by_piece[1]        =     6;
	king_attack_by_piece[2]        =    17;
	king_attack_by_piece[3]        =    12;
	king_attack_by_piece[4]        =    18;
	king_attack_by_piece[5]        =    44;
	king_check_by_piece[2]         =     0;
	king_check_by_piece[3]         =     6;
	king_check_by_piece[4]         =     9;
	king_check_by_piece[5]         =     7;
	king_melee_attack_by_rook      =     7;
	king_melee_attack_by_queen     =    10;
	king_attack_min[0]             =     3;
	king_attack_max[0]             =   459;
	king_attack_rise[0]            =     2;
	king_attack_exponent[0]        =   114;
	king_attack_offset[0]          =    59;
	king_attack_min[1]             =    22;
	king_attack_max[1]             =  1356;
	king_attack_rise[1]            =     1;
	king_attack_exponent[1]        =   131;
	king_attack_offset[1]          =    99;
	center_control.mg()            =     2;
	center_control.eg()            =     1;
	material_imbalance.mg()        =   110;
	material_imbalance.eg()        =    30;
	rule_of_the_square.mg()        =     0;
	rule_of_the_square.eg()        =     9;
	passed_pawn_unhindered.mg()    =     9;
	passed_pawn_unhindered.eg()    =     2;
	hanging_piece[1].mg()          =     4;
	hanging_piece[1].eg()          =     7;
	hanging_piece[2].mg()          =    13;
	hanging_piece[2].eg()          =     0;
	hanging_piece[3].mg()          =     2;
	hanging_piece[3].eg()          =     7;
	hanging_piece[4].mg()          =     7;
	hanging_piece[4].eg()          =    15;
	hanging_piece[5].mg()          =    11;
	hanging_piece[5].eg()          =    11;
	mobility_knight_min.mg()       =   -37;
	mobility_knight_max.mg()       =    69;
	mobility_knight_rise.mg()      =     5;
	mobility_knight_offset.mg()    =     0;
	mobility_bishop_min.mg()       =   -86;
	mobility_bishop_max.mg()       =    16;
	mobility_bishop_rise.mg()      =     6;
	mobility_bishop_offset.mg()    =     0;
	mobility_rook_min.mg()         =  -100;
	mobility_rook_max.mg()         =    73;
	mobility_rook_rise.mg()        =     4;
	mobility_rook_offset.mg()      =     7;
	mobility_queen_min.mg()        =  -100;
	mobility_queen_max.mg()        =    25;
	mobility_queen_rise.mg()       =     2;
	mobility_queen_offset.mg()     =     3;
	mobility_knight_min.eg()       =   -19;
	mobility_knight_max.eg()       =     3;
	mobility_knight_rise.eg()      =     4;
	mobility_knight_offset.eg()    =     0;
	mobility_bishop_min.eg()       =   -62;
	mobility_bishop_max.eg()       =    22;
	mobility_bishop_rise.eg()      =     3;
	mobility_bishop_offset.eg()    =     2;
	mobility_rook_min.eg()         =    -7;
	mobility_rook_max.eg()         =    50;
	mobility_rook_rise.eg()        =     9;
	mobility_rook_offset.eg()      =     2;
	mobility_queen_min.eg()        =   -49;
	mobility_queen_max.eg()        =    13;
	mobility_queen_rise.eg()       =    48;
	mobility_queen_offset.eg()     =     0;
	side_to_move.mg()              =     3;
	side_to_move.eg()              =     1;
	drawishness                    =   -83;
	rooks_on_rank_7.mg()           =    11;
	rooks_on_rank_7.eg()           =    48;
	knight_outposts[0].mg()        =     2;
	knight_outposts[0].eg()        =     6;
	knight_outposts[1].mg()        =    12;
	knight_outposts[1].eg()        =    16;
	bishop_outposts[0].mg()        =     1;
	bishop_outposts[0].eg()        =     4;
	bishop_outposts[1].mg()        =    17;
	bishop_outposts[1].eg()        =     5;

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
			eg_material_values[pieces::knight],
			eg_material_values[pieces::bishop]
		);

	for( int i = 0; i < 6; ++i ) {
		advanced_passed_pawn[i].mg() = double(passed_pawn.mg()) * (1 + std::pow( double(i), double(passed_pawn_advance_power.mg()) / 100.0 ) );
		advanced_passed_pawn[i].eg() = double(passed_pawn.eg()) * (1 + std::pow( double(i), double(passed_pawn_advance_power.eg()) / 100.0 ) );
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
