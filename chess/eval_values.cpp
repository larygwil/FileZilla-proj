#include "chess.hpp"
#include "eval_values.hpp"
#include "tables.hpp"
#include "util.hpp"

#include <algorithm>
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

score mobility_rise[6];
score mobility_min[6];
score mobility_duration[6];

score side_to_move;

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

score mobility[6][32];

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
	material_values[pieces::pawn]   = score( 91, 86 );
	material_values[pieces::knight] = score( 414, 328 );
	material_values[pieces::bishop] = score( 423, 358 );
	material_values[pieces::rook]   = score( 566, 631 );
	material_values[pieces::queen]  = score( 1267, 1157 );
	double_bishop                   = score( 33, 49 );
	passed_pawn_advance_power       = score( 177, 156 );
	passed_pawn_king_distance[0]    = score( 0, 4 );
	passed_pawn_king_distance[1]    = score( 1, 3 );
	passed_pawn_base[0]             = score( 7, 8 );
	passed_pawn_base[1]             = score( 1, 2 );
	passed_pawn_base[2]             = score( 0, 0 );
	passed_pawn_base[3]             = score( 0, 1 );
	doubled_pawn_base[0][0]         = score( 28, 19 );
	doubled_pawn_base[0][1]         = score( 8, 6 );
	doubled_pawn_base[0][2]         = score( 5, 2 );
	doubled_pawn_base[0][3]         = score( 8, 1 );
	doubled_pawn_base[1][0]         = score( 15, 1 );
	doubled_pawn_base[1][1]         = score( 4, 0 );
	doubled_pawn_base[1][2]         = score( 0, 0 );
	doubled_pawn_base[1][3]         = score( 0, 0 );
	backward_pawn_base[0][0]        = score( 11, 1 );
	backward_pawn_base[0][1]        = score( 3, 0 );
	backward_pawn_base[0][2]        = score( 2, 0 );
	backward_pawn_base[0][3]        = score( 2, 0 );
	backward_pawn_base[1][0]        = score( 10, 12 );
	backward_pawn_base[1][1]        = score( 2, 4 );
	backward_pawn_base[1][2]        = score( 0, 0 );
	backward_pawn_base[1][3]        = score( 2, 0 );
	isolated_pawn_base[0][0]        = score( 2, 2 );
	isolated_pawn_base[0][1]        = score( 0, 0 );
	isolated_pawn_base[0][2]        = score( 0, 0 );
	isolated_pawn_base[0][3]        = score( 0, 0 );
	isolated_pawn_base[1][0]        = score( 6, 18 );
	isolated_pawn_base[1][1]        = score( 2, 5 );
	isolated_pawn_base[1][2]        = score( 2, 0 );
	isolated_pawn_base[1][3]        = score( 2, 6 );
	connected_pawn_base[0][0]       = score( 1, 8 );
	connected_pawn_base[0][1]       = score( 0, 1 );
	connected_pawn_base[0][2]       = score( 0, 1 );
	connected_pawn_base[0][3]       = score( 0, 1 );
	connected_pawn_base[1][0]       = score( 9, 1 );
	connected_pawn_base[1][1]       = score( 0, 0 );
	connected_pawn_base[1][2]       = score( 0, 0 );
	connected_pawn_base[1][3]       = score( 0, 0 );
	candidate_passed_pawn_base[0]   = score( 15, 19 );
	candidate_passed_pawn_base[1]   = score( 0, 1 );
	candidate_passed_pawn_base[2]   = score( 5, 5 );
	candidate_passed_pawn_base[3]   = score( 1, 6 );
	pawn_shield[0]                  = score( 16, 9 );
	pawn_shield[1]                  = score( 14, 2 );
	pawn_shield[2]                  = score( 9, 1 );
	pawn_shield[3]                  = score( 7, 1 );
	pawn_shield_attack[0]           = score( 25, 28 );
	pawn_shield_attack[1]           = score( 5, 5 );
	pawn_shield_attack[2]           = score( 5, 5 );
	pawn_shield_attack[3]           = score( 5, 1 );
	absolute_pin[2]                 = score( 44, 40 );
	absolute_pin[3]                 = score( 37, 53 );
	absolute_pin[4]                 = score( 56, 27 );
	absolute_pin[5]                 = score( 47, 1 );
	rooks_on_open_file              = score( 16, 7 );
	rooks_on_half_open_file         = score( 7, 4 );
	connected_rooks                 = score( 8, 7 );
	tropism[1]                      = score( 4, 1 );
	tropism[2]                      = score( 3, 1 );
	tropism[3]                      = score( 1, 1 );
	tropism[4]                      = score( 1, 1 );
	tropism[5]                      = score( 1, 1 );
	king_attack_by_piece[1]         = 1;
	king_attack_by_piece[2]         = 10;
	king_attack_by_piece[3]         = 12;
	king_attack_by_piece[4]         = 19;
	king_attack_by_piece[5]         = 16;
	king_check_by_piece[2]          = 1;
	king_check_by_piece[3]          = 2;
	king_check_by_piece[4]          = 12;
	king_check_by_piece[5]          = 11;
	king_melee_attack_by_rook       = 31;
	king_melee_attack_by_queen      = 50;
	king_attack_pawn_shield         = 12;
	center_control                  = score( 5, 1 );
	material_imbalance[0]           = score( 1, 62 );
	material_imbalance[1]           = score( 22, 115 );
	rule_of_the_square              = score( 5, 7 );
	passed_pawn_unhindered          = score( 9, 6 );
	defended_by_pawn[1]             = score( 0, 11 );
	defended_by_pawn[2]             = score( 3, 6 );
	defended_by_pawn[3]             = score( 3, 0 );
	defended_by_pawn[4]             = score( 4, 0 );
	defended_by_pawn[5]             = score( 0, 0 );
	attacked_piece[1]               = score( 0, 16 );
	attacked_piece[2]               = score( 0, 9 );
	attacked_piece[3]               = score( 2, 10 );
	attacked_piece[4]               = score( 3, 22 );
	attacked_piece[5]               = score( 8, 16 );
	hanging_piece[1]                = score( 1, 18 );
	hanging_piece[2]                = score( 16, 13 );
	hanging_piece[3]                = score( 26, 14 );
	hanging_piece[4]                = score( 6, 33 );
	hanging_piece[5]                = score( 33, 1 );
	mobility_rise[2]                = score( 15, 10 );
	mobility_rise[3]                = score( 9, 12 );
	mobility_rise[4]                = score( 8, 10 );
	mobility_rise[5]                = score( 15, 15 );
	mobility_min[2]                 = score( 2, 3 );
	mobility_min[3]                 = score( 2, 4 );
	mobility_min[4]                 = score( 0, 2 );
	mobility_min[5]                 = score( 24, 0 );
	mobility_duration[2]            = score( 1, 2 );
	mobility_duration[3]            = score( 3, 3 );
	mobility_duration[4]            = score( 1, 7 );
	mobility_duration[5]            = score( 3, 2 );
	side_to_move                    = score( 8, 4 );
	rooks_on_rank_7                 = score( 18, 33 );
	knight_outposts[0]              = score( 11, 10 );
	knight_outposts[1]              = score( 13, 26 );
	bishop_outposts[0]              = score( 5, 13 );
	bishop_outposts[1]              = score( 12, 12 );
	trapped_rook[0].mg()            = -38;
	trapped_rook[1].mg()            = -120;

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

	auto update_mobility = []( int count, score* mobility, score& rise, score& min, score& duration ) {

		auto update_mobility_phase = []( int count, score::phase_ref f, score* mobility, short rise, short min, short duration ) {

			int mid = min + duration / 2;
			(mobility[mid].*f)() = (duration % 2) ? (-rise / 2) : 0;

			for( int i = mid - 1; i >= 0; --i ) {
				(mobility[i].*f)() = (mobility[i + 1].*f)() - ((i >= min) ? rise : 1);
			}

			for( int i = mid + 1; i <= count; ++i ) {
				(mobility[i].*f)() = (mobility[i - 1].*f)() + ((i <= (min + duration)) ? rise : 1);
			}
		};

		update_mobility_phase( count, &score::mg, mobility, rise.mg(), min.mg(), duration.mg() );
		update_mobility_phase( count, &score::eg, mobility, rise.eg(), min.eg(), duration.eg() );
	};

	for( int i = pieces::knight; i <= pieces::queen; ++i ) {
		update_mobility( max_move_count[i], mobility[i], mobility_rise[i], mobility_min[i], mobility_duration[i] );
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

bool sane_base( bool& changed )
{
	auto check_mobility = [&changed]( short count, score& min, score& duration ) {
		
		auto check_mobility_phase = [&changed]( int count, score::value_type& min, score::value_type& duration ) {

			changed |= set_min( min, static_cast<short>(count - 1) );
			changed |= set_min( duration, static_cast<short>(count - min) );
		};

		check_mobility_phase( count, min.mg(), duration.mg() );
		check_mobility_phase( count, min.eg(), duration.eg() );
	};

	for( int i = pieces::knight; i <= pieces::queen; ++i ) {
		check_mobility( max_move_count[i], mobility_min[i], mobility_duration[i] );
	}

	for( int i = 1; i < 5; ++i ) {
		if( material_values[i].mg() >= material_values[i+1].mg() ) {
			return false;
		}
		if( material_values[i].eg() >= material_values[i+1].eg() ) {
			return false;
		}
	}

	return true;
}

bool sane_derived()
{
	return true;
}


bool normalize()
{
	bool changed = false;

	for( int i = 1; i < 4; ++i ) {
		changed |= set_min( passed_pawn_base[i].mg(), static_cast<short>(passed_pawn_base[0].mg() / 3) );
		changed |= set_min( passed_pawn_base[i].eg(), static_cast<short>(passed_pawn_base[0].eg() / 3) );
		changed |= set_min( candidate_passed_pawn_base[i].mg(), static_cast<short>(candidate_passed_pawn_base[0].mg() / 3) );
		changed |= set_min( candidate_passed_pawn_base[i].eg(), static_cast<short>(candidate_passed_pawn_base[0].eg() / 3) );
		for( int c = 0; c < 2; ++c ) {
			changed |= set_min( isolated_pawn_base[c][i].mg(), static_cast<short>(isolated_pawn_base[c][0].mg() / 3) );
			changed |= set_min( isolated_pawn_base[c][i].eg(), static_cast<short>(isolated_pawn_base[c][0].eg() / 3) );
			changed |= set_min( doubled_pawn_base[c][i].mg(), static_cast<short>(doubled_pawn_base[c][0].mg() / 3) );
			changed |= set_min( doubled_pawn_base[c][i].eg(), static_cast<short>(doubled_pawn_base[c][0].eg() / 3) );
			changed |= set_min( connected_pawn_base[c][i].mg(), static_cast<short>(connected_pawn_base[c][0].mg() / 3) );
			changed |= set_min( connected_pawn_base[c][i].eg(), static_cast<short>(connected_pawn_base[c][0].eg() / 3) );
			changed |= set_min( backward_pawn_base[c][i].mg(), static_cast<short>(backward_pawn_base[c][0].mg() / 3) );
			changed |= set_min( backward_pawn_base[c][i].eg(), static_cast<short>(backward_pawn_base[c][0].eg() / 3) );
		}
	}

	for( int i = 0; i < 3; ++i ) {
		changed |= set_min( pawn_shield[i+1].mg(), pawn_shield[i].mg() );
		changed |= set_min( pawn_shield[i+1].eg(), pawn_shield[i].eg() );
		changed |= set_min( pawn_shield_attack[i+1].mg(), pawn_shield_attack[i].mg() );
		changed |= set_min( pawn_shield_attack[i+1].eg(), pawn_shield_attack[i].eg() );
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


piece_square_table pst;


void piece_square_table::update()
{
	for( int i = 0; i < 32; ++i ) {
		ref(color::white, pieces::pawn, (i / 4) * 8 + 7 - i % 4) = pst_data::pawn_values[i];
		ref(color::white, pieces::knight, (i / 4) * 8 + 7 - i % 4) = pst_data::knight_values[i];
		ref(color::white, pieces::bishop, (i / 4) * 8 + 7 - i % 4) = pst_data::bishop_values[i];
		ref(color::white, pieces::rook, (i / 4) * 8 + 7 - i % 4) = pst_data::rook_values[i];
		ref(color::white, pieces::queen, (i / 4) * 8 + 7 - i % 4) = pst_data::queen_values[i];
		ref(color::white, pieces::king, (i / 4) * 8 + 7 - i % 4) = pst_data::king_values[i];
	}

	for( int i = 0; i < 64; ++i ) {
		for( int p = 1; p < 7; ++p ) {
			ref(color::black, static_cast<pieces::type>(p), i) = ref(color::white, static_cast<pieces::type>(p), 63-i);
		}
	}
}


void piece_square_table::init()
{
	for( int i = 0; i < 32; ++i ) {
		ref(color::white, pieces::pawn, (i / 4) * 8 + i % 4) = pst_data::pawn_values[i];
		ref(color::white, pieces::knight, (i / 4) * 8 + i % 4) = pst_data::knight_values[i];
		ref(color::white, pieces::bishop, (i / 4) * 8 + i % 4) = pst_data::bishop_values[i];
		ref(color::white, pieces::rook, (i / 4) * 8 + i % 4) = pst_data::rook_values[i];
		ref(color::white, pieces::queen, (i / 4) * 8 + i % 4) = pst_data::queen_values[i];
		ref(color::white, pieces::king, (i / 4) * 8 + i % 4) = pst_data::king_values[i];
	}

	update();
}
