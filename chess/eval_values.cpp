#include "chess.hpp"
#include "eval_values.hpp"

#include <cmath>

namespace eval_values {

score material_values[7];

score double_bishop;

score passed_pawn_advance_power;
score passed_pawn_king_distance[2];
score doubled_pawn_base[2][4];
score passed_pawn_base[4];
score isolated_pawn_base[2][4];
score connected_pawn_base[2][4];
score candidate_passed_pawn_base[4];
score backward_pawn_base[2][4];

score pawn_shield[4];
score pawn_shield_attack[4];

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

short king_attack_pawn_shield;

score center_control;

short phase_transition_begin;
short phase_transition_duration;

score material_imbalance[2];

score rule_of_the_square;
score passed_pawn_unhindered;

score defended_by_pawn[6];
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

score trapped_rook[2];

// Derived
score initial_material;

int phase_transition_material_begin;
int phase_transition_material_end;

score doubled_pawn[2][8];
score passed_pawn[8];
score isolated_pawn[2][8];
score connected_pawn[2][8];
score candidate_passed_pawn[8];
score backward_pawn[2][8];

score mobility_knight[9];
score mobility_bishop[14];
score mobility_rook[15];
score mobility_queen[7+7+7+6+1];

score king_attack[200];

short insufficient_material_threshold;

score advanced_passed_pawn[8][6];

void init()
{
	// Untweaked values
	material_values[pieces::king] = score( 20000, 20000 );
	king_attack_by_piece[pieces::none] =     0;
	king_check_by_piece[pieces::none]  =     0;
	king_check_by_piece[pieces::pawn]  =     0;

	// Tweaked values
	material_values[pieces::pawn]   = score( 77, 79 );
	material_values[pieces::knight] = score( 320, 334 );
	material_values[pieces::bishop] = score( 356, 413 );
	material_values[pieces::rook]   = score( 572, 623 );
	material_values[pieces::queen]  = score( 1228, 1167 );
	double_bishop                   = score( 52, 33 );
	passed_pawn_advance_power       = score( 207, 152 );
	passed_pawn_king_distance[0]    = score( 0, 6 );
	passed_pawn_king_distance[1]    = score( 1, 4 );
	passed_pawn_base[0]             = score( 5, 12 );
	passed_pawn_base[1]             = score( 0, 4 );
	passed_pawn_base[2]             = score( 1, 0 );
	passed_pawn_base[3]             = score( 1, 0 );
	doubled_pawn_base[0][0]         = score( 30, 5 );
	doubled_pawn_base[0][1]         = score( 3, 1 );
	doubled_pawn_base[0][2]         = score( 3, 1 );
	doubled_pawn_base[0][3]         = score( 8, 1 );
	doubled_pawn_base[1][0]         = score( 9, 1 );
	doubled_pawn_base[1][1]         = score( 3, 0 );
	doubled_pawn_base[1][2]         = score( 3, 0 );
	doubled_pawn_base[1][3]         = score( 0, 0 );
	backward_pawn_base[0][0]        = score( 3, 1 );
	backward_pawn_base[0][1]        = score( 0, 0 );
	backward_pawn_base[0][2]        = score( 1, 0 );
	backward_pawn_base[0][3]        = score( 1, 0 );
	backward_pawn_base[1][0]        = score( 12, 14 );
	backward_pawn_base[1][1]        = score( 0, 3 );
	backward_pawn_base[1][2]        = score( 4, 0 );
	backward_pawn_base[1][3]        = score( 0, 0 );
	isolated_pawn_base[0][0]        = score( 4, 2 );
	isolated_pawn_base[0][1]        = score( 1, 0 );
	isolated_pawn_base[0][2]        = score( 0, 0 );
	isolated_pawn_base[0][3]        = score( 0, 0 );
	isolated_pawn_base[1][0]        = score( 15, 17 );
	isolated_pawn_base[1][1]        = score( 2, 0 );
	isolated_pawn_base[1][2]        = score( 1, 0 );
	isolated_pawn_base[1][3]        = score( 4, 5 );
	connected_pawn_base[0][0]       = score( 1, 4 );
	connected_pawn_base[0][1]       = score( 0, 1 );
	connected_pawn_base[0][2]       = score( 0, 1 );
	connected_pawn_base[0][3]       = score( 0, 0 );
	connected_pawn_base[1][0]       = score( 11, 1 );
	connected_pawn_base[1][1]       = score( 0, 0 );
	connected_pawn_base[1][2]       = score( 0, 0 );
	connected_pawn_base[1][3]       = score( 0, 0 );
	candidate_passed_pawn_base[0]   = score( 18, 27 );
	candidate_passed_pawn_base[1]   = score( 0, 0 );
	candidate_passed_pawn_base[2]   = score( 6, 7 );
	candidate_passed_pawn_base[3]   = score( 6, 9 );
	pawn_shield[0]                  = score( 31, 4 );
	pawn_shield[1]                  = score( 21, 4 );
	pawn_shield[2]                  = score( 11, 1 );
	pawn_shield[3]                  = score( 3, 1 );
	pawn_shield_attack[0]           = score( 34, 34 );
	pawn_shield_attack[1]           = score( 6, 13 );
	pawn_shield_attack[2]           = score( 6, 10 );
	pawn_shield_attack[3]           = score( 6, 1 );
	absolute_pin[2]                 = score( 1, 43 );
	absolute_pin[3]                 = score( 9, 9 );
	absolute_pin[4]                 = score( 15, 1 );
	absolute_pin[5]                 = score( 2, 1 );
	rooks_on_open_file              = score( 23, 15 );
	rooks_on_half_open_file         = score( 10, 7 );
	connected_rooks                 = score( 1, 3 );
	tropism[1]                      = score( 3, 1 );
	tropism[2]                      = score( 3, 1 );
	tropism[3]                      = score( 1, 1 );
	tropism[4]                      = score( 3, 1 );
	tropism[5]                      = score( 1, 1 );
	king_attack_by_piece[1]         = 1;
	king_attack_by_piece[2]         = 12;
	king_attack_by_piece[3]         = 13;
	king_attack_by_piece[4]         = 20;
	king_attack_by_piece[5]         = 16;
	king_check_by_piece[2]          = 1;
	king_check_by_piece[3]          = 6;
	king_check_by_piece[4]          = 1;
	king_check_by_piece[5]          = 3;
	king_melee_attack_by_rook       = 5;
	king_melee_attack_by_queen      = 17;
	king_attack_pawn_shield         = 22;
	center_control                  = score( 4, 1 );
	material_imbalance[0]           = score( 57, 31 );
	material_imbalance[1]           = score( 134, 50 );
	rule_of_the_square              = score( 2, 8 );
	passed_pawn_unhindered          = score( 1, 6 );
	defended_by_pawn[1]             = score( 0, 10 );
	defended_by_pawn[2]             = score( 8, 4 );
	defended_by_pawn[3]             = score( 4, 5 );
	defended_by_pawn[4]             = score( 9, 6 );
	defended_by_pawn[5]             = score( 0, 0 );
	attacked_piece[1]               = score( 1, 18 );
	attacked_piece[2]               = score( 0, 14 );
	attacked_piece[3]               = score( 6, 7 );
	attacked_piece[4]               = score( 13, 18 );
	attacked_piece[5]               = score( 25, 5 );
	hanging_piece[1]                = score( 2, 19 );
	hanging_piece[2]                = score( 9, 10 );
	hanging_piece[3]                = score( 22, 11 );
	hanging_piece[4]                = score( 20, 24 );
	hanging_piece[5]                = score( 28, 10 );
	mobility_knight_min             = score( -3, -13 );
	mobility_knight_max             = score( 32, 37 );
	mobility_knight_rise            = score( 10, 9 );
	mobility_knight_offset          = score( 0, 0 );
	mobility_bishop_min             = score( -25, -60 );
	mobility_bishop_max             = score( 9, 2 );
	mobility_bishop_rise            = score( 9, 9 );
	mobility_bishop_offset          = score( 2, 1 );
	mobility_rook_min               = score( -4, -4 );
	mobility_rook_max               = score( 3, 47 );
	mobility_rook_rise              = score( 6, 23 );
	mobility_rook_offset            = score( 3, 6 );
	mobility_queen_min              = score( -22, -50 );
	mobility_queen_max              = score( 3, 40 );
	mobility_queen_rise             = score( 2, 29 );
	mobility_queen_offset           = score( 14, 1 );
	side_to_move                    = score( 9, 0 );
	rooks_on_rank_7                 = score( 18, 32 );
	knight_outposts[0]              = score( 11, 12 );
	knight_outposts[1]              = score( 17, 30 );
	bishop_outposts[0]              = score( 1, 8 );
	bishop_outposts[1]              = score( 8, 9 );
	trapped_rook[0].mg()            = -16;
	trapped_rook[1].mg()            = -102;

	update_derived();
}

void update_derived()
{
	initial_material =
		material_values[pieces::knight] * 2 +
		material_values[pieces::bishop] * 2 +
		material_values[pieces::rook] * 2 +
		material_values[pieces::queen];

	phase_transition_material_begin = initial_material.mg() * 2;
	phase_transition_material_end = 1000;
	phase_transition_begin = 0;
	phase_transition_duration = phase_transition_material_begin - phase_transition_material_end;

	for( short i = 0; i <= 8; ++i ) {
		if( i > mobility_knight_offset.mg() ) {
			mobility_knight[i].mg() = std::min(short(mobility_knight_min.mg() + mobility_knight_rise.mg() * (i - mobility_knight_offset.mg())), mobility_knight_max.mg());
		}
		else {
			mobility_knight[i].mg() = mobility_knight_min.mg();
		}
		if( i > mobility_knight_offset.eg() ) {
			mobility_knight[i].eg() = std::min(short(mobility_knight_min.eg() + mobility_knight_rise.eg() * (i - mobility_knight_offset.eg())), mobility_knight_max.eg());
		}
		else {
			mobility_knight[i].eg() = mobility_knight_min.eg();
		}
	}

	for( short i = 0; i <= 13; ++i ) {
		if( i > mobility_bishop_offset.mg() ) {
			mobility_bishop[i].mg() = std::min(short(mobility_bishop_min.mg() + mobility_bishop_rise.mg() * (i - mobility_bishop_offset.mg())), mobility_bishop_max.mg());
		}
		else {
			mobility_bishop[i].mg() = mobility_bishop_min.mg();
		}
		if( i > mobility_bishop_offset.eg() ) {
			mobility_bishop[i].eg() = std::min(short(mobility_bishop_min.eg() + mobility_bishop_rise.eg() * (i - mobility_bishop_offset.eg())), mobility_bishop_max.eg());
		}
		else {
			mobility_bishop[i].eg() = mobility_bishop_min.eg();
		}
	}

	for( short i = 0; i <= 14; ++i ) {
		if( i > mobility_rook_offset.mg() ) {
			mobility_rook[i].mg() = std::min(short(mobility_rook_min.mg() + mobility_rook_rise.mg() * (i - mobility_rook_offset.mg())), mobility_rook_max.mg());
		}
		else {
			mobility_rook[i].mg() = mobility_rook_min.mg();
		}
		if( i > mobility_rook_offset.eg() ) {
			mobility_rook[i].eg() = std::min(short(mobility_rook_min.eg() + mobility_rook_rise.eg() * (i - mobility_rook_offset.eg())), mobility_rook_max.eg());
		}
		else {
			mobility_rook[i].eg() = mobility_rook_min.eg();
		}
	}

	for( short i = 0; i <= 27; ++i ) {
		if( i > mobility_queen_offset.mg() ) {
			mobility_queen[i].mg() = std::min(short(mobility_queen_min.mg() + mobility_queen_rise.mg() * (i - mobility_queen_offset.mg())), mobility_queen_max.mg());
		}
		else {
			mobility_queen[i].mg() = mobility_queen_min.mg();
		}
		if( i > mobility_queen_offset.eg() ) {
			mobility_queen[i].eg() = std::min(short(mobility_queen_min.eg() + mobility_queen_rise.eg() * (i - mobility_queen_offset.eg())), mobility_queen_max.eg());
		}
		else {
			mobility_queen[i].eg() = mobility_queen_min.eg();
		}
	}

	for( short i = 0; i < 200; ++i ) {
		short v[2];
		for( int c = 0; c < 2; ++c ) {
			double di = static_cast<double>(i);
			//double dv = di * 0.6f + std::pow( 1.031f, di );
			double dv = 0.7 * di + di * di / 35.;
			if( dv > 500. ) {
				dv = 500.;
			}
			v[c] = static_cast<short>(dv);
		}
		king_attack[i] = score( v[0], v[1] );
	}

	insufficient_material_threshold = std::max(
			material_values[pieces::knight].eg(),
			material_values[pieces::bishop].eg()
		);

	passed_pawn[0] = passed_pawn_base[0];
	candidate_passed_pawn[0] = candidate_passed_pawn_base[0];
	for( int i = 0; i < 2; ++i ) {
		doubled_pawn[i][0] = -doubled_pawn_base[i][0];
		isolated_pawn[i][0] = -isolated_pawn_base[i][0];
		connected_pawn[i][0] = connected_pawn_base[i][0];
		backward_pawn[i][0] = -backward_pawn_base[i][0];
	}

	for( int file = 1; file < 4; ++file ) {
		passed_pawn[file] = passed_pawn[file - 1] + passed_pawn_base[file];
		candidate_passed_pawn[file] = candidate_passed_pawn[file - 1] + candidate_passed_pawn_base[file];
		for( int i = 1; i < 2; ++i ) {
			doubled_pawn[i][file] = doubled_pawn[i][file - 1] - doubled_pawn_base[i][file];
			isolated_pawn[i][file] = isolated_pawn[i][file - 1] - isolated_pawn_base[i][file];
			connected_pawn[i][file] = connected_pawn[i][file - 1] + connected_pawn_base[i][file];
			backward_pawn[i][file] = backward_pawn[i][file - 1] - backward_pawn_base[i][file];
		}
	}

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
			advanced_passed_pawn[file][i].mg() = static_cast<short>(double(passed_pawn[file].mg()) * (1 + std::pow( double(i), double(passed_pawn_advance_power.mg()) / 100.0 ) ));
			advanced_passed_pawn[file][i].eg() = static_cast<short>(double(passed_pawn[file].eg()) * (1 + std::pow( double(i), double(passed_pawn_advance_power.eg()) / 100.0 ) ));
		}
	}
}

bool sane()
{
	for( int i = 1; i < 5; ++i ) {
		if( material_values[i].mg() >= material_values[i+1].mg() ) {
			return false;
		}
		if( material_values[i].eg() >= material_values[i+1].eg() ) {
			return false;
		}
	}
	if( mobility_knight[8].mg() < 0 ) {
		return false;
	}
	if( mobility_knight[8].eg() < 0 ) {
		return false;
	}
	if( mobility_bishop[13].mg() < 0 ) {
		return false;
	}
	if( mobility_bishop[13].eg() < 0 ) {
		return false;
	}
	if( mobility_rook[14].mg() < 0 ) {
		return false;
	}
	if( mobility_rook[14].eg() < 0 ) {
		return false;
	}
	if( mobility_queen[27].mg() < 0 ) {
		return false;
	}
	if( mobility_queen[27].eg() < 0 ) {
		return false;
	}

	return true;
}

bool normalize()
{
	bool changed = false;
	for( int i = 1; i < 4; ++i ) {
		if( passed_pawn_base[i].mg() > passed_pawn_base[0].mg() / 3 ) {
			passed_pawn_base[i].mg() = passed_pawn_base[0].mg() / 3;
			changed = true;
		}
		if( passed_pawn_base[i].eg() > passed_pawn_base[0].eg() / 3 ) {
			passed_pawn_base[i].eg() = passed_pawn_base[0].eg() / 3;
			changed = true;
		}
		if( candidate_passed_pawn_base[i].mg() > candidate_passed_pawn_base[0].mg() / 3 ) {
			candidate_passed_pawn_base[i].mg() = candidate_passed_pawn_base[0].mg() / 3;
			changed = true;
		}
		if( candidate_passed_pawn_base[i].eg() > candidate_passed_pawn_base[0].eg() / 3 ) {
			candidate_passed_pawn_base[i].eg() = candidate_passed_pawn_base[0].eg() / 3;
			changed = true;
		}
		for( int c = 0; c < 2; ++c ) {
			if( isolated_pawn_base[c][i].mg() > isolated_pawn_base[c][0].mg() / 3 ) {
				isolated_pawn_base[c][i].mg() = isolated_pawn_base[c][0].mg() / 3;
				changed = true;
			}
			if( isolated_pawn_base[c][i].eg() > isolated_pawn_base[c][0].eg() / 3 ) {
				isolated_pawn_base[c][i].eg() = isolated_pawn_base[c][0].eg() / 3;
				changed = true;
			}
			if( doubled_pawn_base[c][i].mg() > doubled_pawn_base[c][0].mg() / 3 ) {
				doubled_pawn_base[c][i].mg() = doubled_pawn_base[c][0].mg() / 3;
				changed = true;
			}
			if( doubled_pawn_base[c][i].eg() > doubled_pawn_base[c][0].eg() / 3 ) {
				doubled_pawn_base[c][i].eg() = doubled_pawn_base[c][0].eg() / 3;
				changed = true;
			}
			if( connected_pawn_base[c][i].mg() > connected_pawn_base[c][0].mg() / 3 ) {
				connected_pawn_base[c][i].mg() = connected_pawn_base[c][0].mg() / 3;
				changed = true;
			}
			if( connected_pawn_base[c][i].eg() > connected_pawn_base[c][0].eg() / 3 ) {
				connected_pawn_base[c][i].eg() = connected_pawn_base[c][0].eg() / 3;
				changed = true;
			}
			if( backward_pawn_base[c][i].mg() > backward_pawn_base[c][0].mg() / 3 ) {
				backward_pawn_base[c][i].mg() = backward_pawn_base[c][0].mg() / 3;
				changed = true;
			}
			if( backward_pawn_base[c][i].eg() > backward_pawn_base[c][0].eg() / 3 ) {
				backward_pawn_base[c][i].eg() = backward_pawn_base[c][0].eg() / 3;
				changed = true;
			}
		}
	}
	if( mobility_knight_max.mg() > mobility_knight[8].mg() ) {
		mobility_knight_max.mg() = mobility_knight[8].mg();
		changed = true;
	}
	if( mobility_knight_max.eg() > mobility_knight[8].eg() ) {
		mobility_knight_max.eg() = mobility_knight[8].eg();
		changed = true;
	}
	if( mobility_bishop_max.mg() > mobility_bishop[13].mg() ) {
		mobility_bishop_max.mg() = mobility_bishop[13].mg();
		changed = true;
	}
	if( mobility_bishop_max.eg() > mobility_bishop[13].eg() ) {
		mobility_bishop_max.eg() = mobility_bishop[13].eg();
		changed = true;
	}
	if( mobility_rook_max.mg() > mobility_rook[14].mg() ) {
		mobility_rook_max.mg() = mobility_rook[14].mg();
		changed = true;
	}
	if( mobility_rook_max.eg() > mobility_rook[14].eg() ) {
		mobility_rook_max.eg() = mobility_rook[14].eg();
		changed = true;
	}
	if( mobility_queen_max.mg() > mobility_queen[27].mg() ) {
		mobility_queen_max.mg() = mobility_queen[27].mg();
		changed = true;
	}
	if( mobility_queen_max.eg() > mobility_queen[27].eg() ) {
		mobility_queen_max.eg() = mobility_queen[27].eg();
		changed = true;
	}
	for( int i = 1; i < 4; ++i ) {
		if( pawn_shield[i].mg() > pawn_shield[i-1].mg() ) {
			pawn_shield[i].mg() = pawn_shield[i-1].mg();
			changed = true;
		}
		if( pawn_shield[i].eg() > pawn_shield[i-1].eg() ) {
			pawn_shield[i].eg() = pawn_shield[i-1].eg();
			changed = true;
		}
		if( pawn_shield_attack[i].mg() > pawn_shield_attack[i-1].mg() ) {
			pawn_shield_attack[i].mg() = pawn_shield_attack[i-1].mg();
			changed = true;
		}
		if( pawn_shield_attack[i].eg() > pawn_shield_attack[i-1].eg() ) {
			pawn_shield_attack[i].eg() = pawn_shield_attack[i-1].eg();
			changed = true;
		}
	}

	return changed;
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
	s(  0, 0), s( 0,0), s( 0,0), s( 0,0)
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


void update_pst()
{
	for( int i = 0; i < 32; ++i ) {
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

void init_pst()
{
	for( int i = 0; i < 32; ++i ) {
		pst[0][1][(i / 4) * 8 + i % 4] = pst_data::pawn_values[i];
		pst[0][2][(i / 4) * 8 + i % 4] = pst_data::knight_values[i];
		pst[0][3][(i / 4) * 8 + i % 4] = pst_data::bishop_values[i];
		pst[0][4][(i / 4) * 8 + i % 4] = pst_data::rook_values[i];
		pst[0][5][(i / 4) * 8 + i % 4] = pst_data::queen_values[i];
		pst[0][6][(i / 4) * 8 + i % 4] = pst_data::king_values[i];
	}

	update_pst();
}
