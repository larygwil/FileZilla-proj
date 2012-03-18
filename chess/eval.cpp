#include "eval.hpp"
#include "assert.hpp"
#include "mobility.hpp"
#include "util.hpp"
#include "pawn_structure_hash_table.hpp"
#include "tables.hpp"
#include "zobrist.hpp"

#include <iostream>
#include <cmath>

extern uint64_t const king_pawn_shield[2][64];
extern uint64_t const isolated_pawns[64];

eval_values_t eval_values;

eval_values_t::eval_values_t()
{
	// Untweaked values
	mg_material_values[pieces::none] =      0;
	mg_material_values[pieces::king] =  20000;
	eg_material_values[pieces::none] =      0;
	eg_material_values[pieces::king] =  20000;
	king_attack_by_piece[pieces::none] = 0;
	king_check_by_piece[pieces::none] = 0;
	king_check_by_piece[pieces::pawn] = 0;
	castled                     =     0;

	// Tweaked values
	mg_material_values[1]          =   101;
	mg_material_values[2]          =   308;
	mg_material_values[3]          =   273;
	mg_material_values[4]          =   557;
	mg_material_values[5]          =   966;
	eg_material_values[1]          =    86;
	eg_material_values[2]          =   406;
	eg_material_values[3]          =   365;
	eg_material_values[4]          =   496;
	eg_material_values[5]          =  1034;
	double_bishop.mg()             =    17;
	double_bishop.eg()             =    33;
	doubled_pawn.mg()              =     0;
	passed_pawn.mg()               =     5;
	isolated_pawn.mg()             =   -16;
	connected_pawn.mg()            =     0;
	doubled_pawn.eg()              =   -27;
	passed_pawn.eg()               =    11;
	isolated_pawn.eg()             =   -11;
	connected_pawn.eg()            =     1;
	pawn_shield.mg()               =     0;
	pawn_shield.eg()               =     8;
	pin_absolute_bishop.mg()       =    50;
	pin_absolute_bishop.eg()       =     0;
	pin_absolute_rook.mg()         =    17;
	pin_absolute_rook.eg()         =    47;
	pin_absolute_queen.mg()        =     0;
	pin_absolute_queen.eg()        =     0;
	rooks_on_open_file.mg()        =     0;
	rooks_on_open_file.eg()        =    37;
	rooks_on_half_open_file.mg()   =     4;
	rooks_on_half_open_file.eg()   =    15;
	connected_rooks.mg()           =    40;
	connected_rooks.eg()           =    21;
	tropism.mg()                   =     0;
	tropism.eg()                   =     3;
	king_attack_by_piece[1]        =     3;
	king_attack_by_piece[2]        =    32;
	king_attack_by_piece[3]        =     0;
	king_attack_by_piece[4]        =     6;
	king_attack_by_piece[5]        =    22;
	king_check_by_piece[2]         =    12;
	king_check_by_piece[3]         =     3;
	king_check_by_piece[4]         =     0;
	king_check_by_piece[5]         =     9;
	king_melee_attack_by_rook      =     5;
	king_melee_attack_by_queen     =     9;
	king_attack_min[0]             =    89;
	king_attack_max[0]             =   239;
	king_attack_rise[0]            =    22;
	king_attack_exponent[0]        =   159;
	king_attack_offset[0]          =    91;
	king_attack_min[1]             =    78;
	king_attack_max[1]             =   159;
	king_attack_rise[1]            =     4;
	king_attack_exponent[1]        =   105;
	king_attack_offset[1]          =    66;
	center_control.mg()            =     4;
	center_control.eg()            =     1;
	material_imbalance.mg()        =   111;
	material_imbalance.eg()        =    19;
	rule_of_the_square.mg()        =    40;
	rule_of_the_square.eg()        =    15;
	passed_pawn_unhindered.mg()    =    19;
	passed_pawn_unhindered.eg()    =     0;
	hanging_piece[1].mg()          =     1;
	hanging_piece[1].eg()          =     4;
	hanging_piece[2].mg()          =    36;
	hanging_piece[2].eg()          =     9;
	hanging_piece[3].mg()          =    28;
	hanging_piece[3].eg()          =     7;
	hanging_piece[4].mg()          =    67;
	hanging_piece[4].eg()          =     0;
	hanging_piece[5].mg()          =     0;
	hanging_piece[5].eg()          =     0;
	mobility_knight_min.mg()       =   -95;
	mobility_knight_max.mg()       =    42;
	mobility_knight_rise.mg()      =    18;
	mobility_knight_offset.mg()    =     4;
	mobility_bishop_min.mg()       =   -46;
	mobility_bishop_max.mg()       =    17;
	mobility_bishop_rise.mg()      =     9;
	mobility_bishop_offset.mg()    =     3;
	mobility_rook_min.mg()         =   -64;
	mobility_rook_max.mg()         =    91;
	mobility_rook_rise.mg()        =     1;
	mobility_rook_offset.mg()      =     1;
	mobility_queen_min.mg()        =     0;
	mobility_queen_max.mg()        =    24;
	mobility_queen_rise.mg()       =    25;
	mobility_queen_offset.mg()     =     6;
	mobility_knight_min.eg()       =   -96;
	mobility_knight_max.eg()       =    98;
	mobility_knight_rise.eg()      =    13;
	mobility_knight_offset.eg()    =     2;
	mobility_bishop_min.eg()       =   -26;
	mobility_bishop_max.eg()       =     4;
	mobility_bishop_rise.eg()      =     6;
	mobility_bishop_offset.eg()    =     0;
	mobility_rook_min.eg()         =   -80;
	mobility_rook_max.eg()         =     6;
	mobility_rook_rise.eg()        =     6;
	mobility_rook_offset.eg()      =     1;
	mobility_queen_min.eg()        =    -8;
	mobility_queen_max.eg()        =    77;
	mobility_queen_rise.eg()       =    43;
	mobility_queen_offset.eg()     =     0;
	side_to_move.mg()              =     0;
	side_to_move.eg()              =     6;

	update_derived();
}

void eval_values_t::update_derived()
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

	for( short i = 0; i < 8; ++i ) {
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

	for( short i = 0; i < 13; ++i ) {
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

	for( short i = 0; i < 14; ++i ) {
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

	for( short i = 0; i < 27; ++i ) {
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

	for( short i = 0; i < 150; ++i ) {
		short v[2];
		for( int c = 0; c < 2; ++c ) {
			if( i > king_attack_offset[c] ) {
				double factor = i - king_attack_offset[c];
				factor = std::pow( factor, double(eval_values.king_attack_exponent[c]) / 100.0 );
				v[c] = (std::min)(short(king_attack_min[c] + king_attack_rise[c] * factor), short(king_attack_min[c] + king_attack_max[c]));
			}
			else {
				v[c] = 0;
			}
		}
		king_attack[i] = score( v[0], v[1] );
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
	s(  0, 0), s(0,0), s( 0,0), s( 0,0),
	s(-10, 0), s(0,0), s( 0,0), s( 0,0),
	s( -8, 0), s(3,0), s( 5,0), s( 5,0),
	s( -5, 0), s(5,0), s(10,0), s(15,0),
	s( -5 ,0), s(2,0), s( 8,0), s(10,0),
	s( -5, 0), s(0,0), s( 3,0), s( 5,0),
	s( -5, 0), s(0,0), s( 3,0), s( 5,0),
	s(  0, 0), s(0,0), s( 0,0), s( 0,0)
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
	s( 45,-50), s( 50,-30), s( 40,-10), s( 20,  0),
	s( 40,-30), s( 45,-10), s( 25,  0), s( 15, 10),
	s( 20,-10), s( 25,  0), s( 15, 10), s( -5, 30),
	s( 15,  0), s( 20, 10), s(  0, 30), s(-15, 50),
	s(  0,  0), s( 15, 10), s( -5, 30), s(-20, 50),
	s( -5,-10), s(  0,  0), s(-15, 10), s(-30, 30),
	s(-15,-30), s( -5,-10), s(-20,  0), s(-35, 10),
	s(-20,-50), s(-15,-30), s(-30,-10), s(-40,  0)
};
}

score pst[2][7][64];


void init_pst()
{
	for( int i = 0; i < 32; ++i ) {
		pst[0][1][i] = pst_data::pawn_values[i];
		pst[0][2][i] = pst_data::knight_values[i];
		pst[0][3][i] = pst_data::bishop_values[i];
		pst[0][4][i] = pst_data::rook_values[i];
		pst[0][5][i] = pst_data::queen_values[i];
		pst[0][6][i] = pst_data::king_values[i];

		for( int p = 1; p < 7; ++p ) {
			pst[0][p][i+7-2*(i%8)] = pst[0][p][i];
		}
	}

	for( int i = 0; i < 64; ++i ) {
		for( int p = 1; p < 7; ++p ) {
			pst[1][p][i] = pst[0][p][63-i];
		}
	}
}


extern uint64_t const passed_pawns[2][64] = {
{
		0x0303030303030300ull,
		0x0707070707070700ull,
		0x0e0e0e0e0e0e0e00ull,
		0x1c1c1c1c1c1c1c00ull,
		0x3838383838383800ull,
		0x7070707070707000ull,
		0xe0e0e0e0e0e0e000ull,
		0xc0c0c0c0c0c0c000ull,
		0x0303030303030000ull,
		0x0707070707070000ull,
		0x0e0e0e0e0e0e0000ull,
		0x1c1c1c1c1c1c0000ull,
		0x3838383838380000ull,
		0x7070707070700000ull,
		0xe0e0e0e0e0e00000ull,
		0xc0c0c0c0c0c00000ull,
		0x0303030303000000ull,
		0x0707070707000000ull,
		0x0e0e0e0e0e000000ull,
		0x1c1c1c1c1c000000ull,
		0x3838383838000000ull,
		0x7070707070000000ull,
		0xe0e0e0e0e0000000ull,
		0xc0c0c0c0c0000000ull,
		0x0303030300000000ull,
		0x0707070700000000ull,
		0x0e0e0e0e00000000ull,
		0x1c1c1c1c00000000ull,
		0x3838383800000000ull,
		0x7070707000000000ull,
		0xe0e0e0e000000000ull,
		0xc0c0c0c000000000ull,
		0x0303030000000000ull,
		0x0707070000000000ull,
		0x0e0e0e0000000000ull,
		0x1c1c1c0000000000ull,
		0x3838380000000000ull,
		0x7070700000000000ull,
		0xe0e0e00000000000ull,
		0xc0c0c00000000000ull,
		0x0303000000000000ull,
		0x0707000000000000ull,
		0x0e0e000000000000ull,
		0x1c1c000000000000ull,
		0x3838000000000000ull,
		0x7070000000000000ull,
		0xe0e0000000000000ull,
		0xc0c0000000000000ull,
		0x0300000000000000ull,
		0x0700000000000000ull,
		0x0e00000000000000ull,
		0x1c00000000000000ull,
		0x3800000000000000ull,
		0x7000000000000000ull,
		0xe000000000000000ull,
		0xc000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull
	},
{
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000003ull,
		0x0000000000000007ull,
		0x000000000000000eull,
		0x000000000000001cull,
		0x0000000000000038ull,
		0x0000000000000070ull,
		0x00000000000000e0ull,
		0x00000000000000c0ull,
		0x0000000000000303ull,
		0x0000000000000707ull,
		0x0000000000000e0eull,
		0x0000000000001c1cull,
		0x0000000000003838ull,
		0x0000000000007070ull,
		0x000000000000e0e0ull,
		0x000000000000c0c0ull,
		0x0000000000030303ull,
		0x0000000000070707ull,
		0x00000000000e0e0eull,
		0x00000000001c1c1cull,
		0x0000000000383838ull,
		0x0000000000707070ull,
		0x0000000000e0e0e0ull,
		0x0000000000c0c0c0ull,
		0x0000000003030303ull,
		0x0000000007070707ull,
		0x000000000e0e0e0eull,
		0x000000001c1c1c1cull,
		0x0000000038383838ull,
		0x0000000070707070ull,
		0x00000000e0e0e0e0ull,
		0x00000000c0c0c0c0ull,
		0x0000000303030303ull,
		0x0000000707070707ull,
		0x0000000e0e0e0e0eull,
		0x0000001c1c1c1c1cull,
		0x0000003838383838ull,
		0x0000007070707070ull,
		0x000000e0e0e0e0e0ull,
		0x000000c0c0c0c0c0ull,
		0x0000030303030303ull,
		0x0000070707070707ull,
		0x00000e0e0e0e0e0eull,
		0x00001c1c1c1c1c1cull,
		0x0000383838383838ull,
		0x0000707070707070ull,
		0x0000e0e0e0e0e0e0ull,
		0x0000c0c0c0c0c0c0ull,
		0x0003030303030303ull,
		0x0007070707070707ull,
		0x000e0e0e0e0e0e0eull,
		0x001c1c1c1c1c1c1cull,
		0x0038383838383838ull,
		0x0070707070707070ull,
		0x00e0e0e0e0e0e0e0ull,
		0x00c0c0c0c0c0c0c0ull
	}
};

extern uint64_t const doubled_pawns[2][64] = {
	{
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000001ull,
		0x0000000000000002ull,
		0x0000000000000004ull,
		0x0000000000000008ull,
		0x0000000000000010ull,
		0x0000000000000020ull,
		0x0000000000000040ull,
		0x0000000000000080ull,
		0x0000000000000101ull,
		0x0000000000000202ull,
		0x0000000000000404ull,
		0x0000000000000808ull,
		0x0000000000001010ull,
		0x0000000000002020ull,
		0x0000000000004040ull,
		0x0000000000008080ull,
		0x0000000000010101ull,
		0x0000000000020202ull,
		0x0000000000040404ull,
		0x0000000000080808ull,
		0x0000000000101010ull,
		0x0000000000202020ull,
		0x0000000000404040ull,
		0x0000000000808080ull,
		0x0000000001010101ull,
		0x0000000002020202ull,
		0x0000000004040404ull,
		0x0000000008080808ull,
		0x0000000010101010ull,
		0x0000000020202020ull,
		0x0000000040404040ull,
		0x0000000080808080ull,
		0x0000000101010101ull,
		0x0000000202020202ull,
		0x0000000404040404ull,
		0x0000000808080808ull,
		0x0000001010101010ull,
		0x0000002020202020ull,
		0x0000004040404040ull,
		0x0000008080808080ull,
		0x0000010101010101ull,
		0x0000020202020202ull,
		0x0000040404040404ull,
		0x0000080808080808ull,
		0x0000101010101010ull,
		0x0000202020202020ull,
		0x0000404040404040ull,
		0x0000808080808080ull,
		0x0001010101010101ull,
		0x0002020202020202ull,
		0x0004040404040404ull,
		0x0008080808080808ull,
		0x0010101010101010ull,
		0x0020202020202020ull,
		0x0040404040404040ull,
		0x0080808080808080ull
	},
	{
		0x0101010101010100ull,
		0x0202020202020200ull,
		0x0404040404040400ull,
		0x0808080808080800ull,
		0x1010101010101000ull,
		0x2020202020202000ull,
		0x4040404040404000ull,
		0x8080808080808000ull,
		0x0101010101010000ull,
		0x0202020202020000ull,
		0x0404040404040000ull,
		0x0808080808080000ull,
		0x1010101010100000ull,
		0x2020202020200000ull,
		0x4040404040400000ull,
		0x8080808080800000ull,
		0x0101010101000000ull,
		0x0202020202000000ull,
		0x0404040404000000ull,
		0x0808080808000000ull,
		0x1010101010000000ull,
		0x2020202020000000ull,
		0x4040404040000000ull,
		0x8080808080000000ull,
		0x0101010100000000ull,
		0x0202020200000000ull,
		0x0404040400000000ull,
		0x0808080800000000ull,
		0x1010101000000000ull,
		0x2020202000000000ull,
		0x4040404000000000ull,
		0x8080808000000000ull,
		0x0101010000000000ull,
		0x0202020000000000ull,
		0x0404040000000000ull,
		0x0808080000000000ull,
		0x1010100000000000ull,
		0x2020200000000000ull,
		0x4040400000000000ull,
		0x8080800000000000ull,
		0x0101000000000000ull,
		0x0202000000000000ull,
		0x0404000000000000ull,
		0x0808000000000000ull,
		0x1010000000000000ull,
		0x2020000000000000ull,
		0x4040000000000000ull,
		0x8080000000000000ull,
		0x0100000000000000ull,
		0x0200000000000000ull,
		0x0400000000000000ull,
		0x0800000000000000ull,
		0x1000000000000000ull,
		0x2000000000000000ull,
		0x4000000000000000ull,
		0x8000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull
	}
};

namespace {
uint64_t const connected_pawns[2][64] = {
{
		0x0000000000000202ull,
		0x0000000000000505ull,
		0x0000000000000a0aull,
		0x0000000000001414ull,
		0x0000000000002828ull,
		0x0000000000005050ull,
		0x000000000000a0a0ull,
		0x0000000000004040ull,
		0x0000000000020202ull,
		0x0000000000050505ull,
		0x00000000000a0a0aull,
		0x0000000000141414ull,
		0x0000000000282828ull,
		0x0000000000505050ull,
		0x0000000000a0a0a0ull,
		0x0000000000404040ull,
		0x0000000002020200ull,
		0x0000000005050500ull,
		0x000000000a0a0a00ull,
		0x0000000014141400ull,
		0x0000000028282800ull,
		0x0000000050505000ull,
		0x00000000a0a0a000ull,
		0x0000000040404000ull,
		0x0000000202020000ull,
		0x0000000505050000ull,
		0x0000000a0a0a0000ull,
		0x0000001414140000ull,
		0x0000002828280000ull,
		0x0000005050500000ull,
		0x000000a0a0a00000ull,
		0x0000004040400000ull,
		0x0000020202000000ull,
		0x0000050505000000ull,
		0x00000a0a0a000000ull,
		0x0000141414000000ull,
		0x0000282828000000ull,
		0x0000505050000000ull,
		0x0000a0a0a0000000ull,
		0x0000404040000000ull,
		0x0002020200000000ull,
		0x0005050500000000ull,
		0x000a0a0a00000000ull,
		0x0014141400000000ull,
		0x0028282800000000ull,
		0x0050505000000000ull,
		0x00a0a0a000000000ull,
		0x0040404000000000ull,
		0x0202020000000000ull,
		0x0505050000000000ull,
		0x0a0a0a0000000000ull,
		0x1414140000000000ull,
		0x2828280000000000ull,
		0x5050500000000000ull,
		0xa0a0a00000000000ull,
		0x4040400000000000ull,
		0x0202000000000000ull,
		0x0505000000000000ull,
		0x0a0a000000000000ull,
		0x1414000000000000ull,
		0x2828000000000000ull,
		0x5050000000000000ull,
		0xa0a0000000000000ull,
		0x4040000000000000ull
	},
{
		0x0000000000000202ull,
		0x0000000000000505ull,
		0x0000000000000a0aull,
		0x0000000000001414ull,
		0x0000000000002828ull,
		0x0000000000005050ull,
		0x000000000000a0a0ull,
		0x0000000000004040ull,
		0x0000000000020202ull,
		0x0000000000050505ull,
		0x00000000000a0a0aull,
		0x0000000000141414ull,
		0x0000000000282828ull,
		0x0000000000505050ull,
		0x0000000000a0a0a0ull,
		0x0000000000404040ull,
		0x0000000002020200ull,
		0x0000000005050500ull,
		0x000000000a0a0a00ull,
		0x0000000014141400ull,
		0x0000000028282800ull,
		0x0000000050505000ull,
		0x00000000a0a0a000ull,
		0x0000000040404000ull,
		0x0000000202020000ull,
		0x0000000505050000ull,
		0x0000000a0a0a0000ull,
		0x0000001414140000ull,
		0x0000002828280000ull,
		0x0000005050500000ull,
		0x000000a0a0a00000ull,
		0x0000004040400000ull,
		0x0000020202000000ull,
		0x0000050505000000ull,
		0x00000a0a0a000000ull,
		0x0000141414000000ull,
		0x0000282828000000ull,
		0x0000505050000000ull,
		0x0000a0a0a0000000ull,
		0x0000404040000000ull,
		0x0002020200000000ull,
		0x0005050500000000ull,
		0x000a0a0a00000000ull,
		0x0014141400000000ull,
		0x0028282800000000ull,
		0x0050505000000000ull,
		0x00a0a0a000000000ull,
		0x0040404000000000ull,
		0x0202020000000000ull,
		0x0505050000000000ull,
		0x0a0a0a0000000000ull,
		0x1414140000000000ull,
		0x2828280000000000ull,
		0x5050500000000000ull,
		0xa0a0a00000000000ull,
		0x4040400000000000ull,
		0x0202000000000000ull,
		0x0505000000000000ull,
		0x0a0a000000000000ull,
		0x1414000000000000ull,
		0x2828000000000000ull,
		0x5050000000000000ull,
		0xa0a0000000000000ull,
		0x4040000000000000ull
	}
};


void evaluate_pawn( uint64_t own_pawns, uint64_t foreign_pawns, color::type c, uint64_t pawn,
					 uint64_t& unpassed, uint64_t& doubled, uint64_t& connected, uint64_t& unisolated )
{
	doubled |= doubled_pawns[c][pawn] & own_pawns;
	unpassed |= passed_pawns[c][pawn] & foreign_pawns;
	connected |= connected_pawns[c][pawn] & own_pawns;
	unisolated |= isolated_pawns[pawn] & own_pawns;
}
}

score evaluate_pawns( uint64_t white_pawns, uint64_t black_pawns, uint64_t& passed )
{
	// Two while loops, otherwise nice branchless solution.

	uint64_t unpassed_white = 0;
	uint64_t doubled_white = 0;
	uint64_t connected_white = 0;
	uint64_t unisolated_white = 0;
	uint64_t unpassed_black = 0;
	uint64_t doubled_black = 0;
	uint64_t connected_black = 0;
	uint64_t unisolated_black = 0;
	{

		uint64_t pawns = white_pawns;
		while( pawns ) {
			uint64_t pawn = bitscan_unset( pawns );

			evaluate_pawn( white_pawns, black_pawns, color::white, pawn,
								  unpassed_black, doubled_white, connected_white, unisolated_white );

		}
	}


	{
		uint64_t pawns = black_pawns;
		while( pawns ) {
			uint64_t pawn = bitscan_unset( pawns );

			evaluate_pawn( black_pawns, white_pawns, color::black, pawn,
						   unpassed_white, doubled_black, connected_black, unisolated_black );

		}
	}
	unpassed_white |= doubled_white;
	unpassed_black |= doubled_black;

	uint64_t passed_white = white_pawns ^ unpassed_white;
	uint64_t passed_black = black_pawns ^ unpassed_black;
	passed = passed_white | passed_black;

	score eval;
	eval = eval_values.passed_pawn * static_cast<short>(popcount(passed_white));
	eval += eval_values.doubled_pawn * static_cast<short>(popcount(doubled_white));
	eval += eval_values.connected_pawn * static_cast<short>(popcount(connected_white));
	eval += eval_values.isolated_pawn * static_cast<short>(popcount(white_pawns ^ unisolated_white));
	eval -= eval_values.passed_pawn * static_cast<short>(popcount(passed_black));
	eval -= eval_values.doubled_pawn * static_cast<short>(popcount(doubled_black));
	eval -= eval_values.connected_pawn * static_cast<short>(popcount(connected_black));
	eval -= eval_values.isolated_pawn * static_cast<short>(popcount(black_pawns ^ unisolated_black));

	return eval;
}


short evaluate_move( position const& p, color::type c, move const& m )
{
	score delta = -pst[c][m.piece][m.source];

	if( m.flags & move_flags::castle ) {
		delta += pst[c][pieces::king][m.target];

		unsigned char row = c ? 56 : 0;
		if( m.target % 8 == 6 ) {
			// Kingside
			delta += pst[c][pieces::rook][row + 5] - pst[c][pieces::rook][row + 7];
		}
		else {
			// Queenside
			delta += pst[c][pieces::rook][row + 3] - pst[c][pieces::rook][row];
		}
	}
	else {

		if( m.captured_piece != pieces::none ) {
			if( m.flags & move_flags::enpassant ) {
				unsigned char ep = (m.target & 0x7) | (m.source & 0x38);
				delta += eval_values.material_values[pieces::pawn] + pst[1-c][pieces::pawn][ep];
			}
			else {
				delta += eval_values.material_values[m.captured_piece] + pst[1-c][m.captured_piece][m.target];
			}
		}

		int promotion = m.flags & move_flags::promotion_mask;
		if( promotion ) {
			pieces::type promotion_piece = static_cast<pieces::type>(promotion >> move_flags::promotion_shift);
			delta -= eval_values.material_values[pieces::pawn];
			delta += eval_values.material_values[promotion_piece] + pst[c][promotion_piece][m.target];
		}
		else {
			delta += pst[c][m.piece][m.target];
		}
	}

#if 0
	{
		position p2 = p;
		apply_move( p2, m, c );
		ASSERT( p2.verify() );
		ASSERT( p.base_eval + delta == p2.base_eval );
	}
#endif

	return delta.scale( p.material[0].mg() + p.material[1].mg() );
}


/* Pawn shield for king
 * 1 2 3
 * 4 5 6
 *   K
 * Pawns on 1, 3, 4, 6 are awarded shield_const points, pawns on 2 and 5 are awarded twice as many points.
 * Double pawns do not count.
 *
 * Special case: a and h file. There b and g also count twice.
 */
static score evaluate_pawn_shield_side( position const& p, color::type c )
{
	uint64_t kings = p.bitboards[c].b[bb_type::king];
	uint64_t king = bitscan( kings );

	uint64_t shield = king_pawn_shield[c][king] & p.bitboards[c].b[bb_type::pawns];
	
	return eval_values.pawn_shield * static_cast<short>(popcount(shield));
}


score evaluate_pawn_shield( position const& p )
{
	return evaluate_pawn_shield_side( p, color::white ) - evaluate_pawn_shield_side( p, color::black );
}


short evaluate_full( position const& p, color::type c )
{
	score base_eval = p.base_eval;
	
	uint64_t passed_pawns;
	base_eval += evaluate_pawns( p.bitboards[color::white].b[bb_type::pawns], p.bitboards[color::black].b[bb_type::pawns], passed_pawns );

	if( popcount( p.bitboards[color::white].b[bb_type::bishops] ) > 1 ) {
		base_eval += eval_values.double_bishop;
	}
	if( popcount( p.bitboards[color::black].b[bb_type::bishops] ) > 1 ) {
		base_eval -= eval_values.double_bishop;
	}

	// Todo: Make truly phased.
	short mat[2];
	mat[0] = static_cast<short>( popcount( p.bitboards[0].b[bb_type::all_pieces] ^ p.bitboards[0].b[bb_type::pawns] ) );
	mat[1] = static_cast<short>( popcount( p.bitboards[1].b[bb_type::all_pieces] ^ p.bitboards[1].b[bb_type::pawns] ) );
	short diff = mat[0] - mat[1];
	base_eval += (eval_values.material_imbalance * diff);

	base_eval += evaluate_pawn_shield( p );

	base_eval += evaluate_mobility( p, c, passed_pawns );

	if( c ) {
		base_eval -= eval_values.side_to_move;
	}
	else {
		base_eval += eval_values.side_to_move;
	}
	
	short eval = base_eval.scale( p.material[0].mg() + p.material[1].mg() );
	if( c ) {
		eval = -eval;
	}

	ASSERT( eval > result::loss && eval < result::win );

	return eval;
}

score get_material_value( pieces::type pi )
{
	return eval_values.material_values[pi];
}
