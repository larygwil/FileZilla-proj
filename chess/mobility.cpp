#include "mobility.hpp"
#include "tables.hpp"
#include "eval.hpp"
#include "magic.hpp"
#include "sliding_piece_attacks.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

uint64_t const center_squares = 0x00003c3c3c3c0000ull;

extern uint64_t const passed_pawns[2][64];
extern uint64_t const doubled_pawns[2][64];
namespace {

struct eval_results {
	eval_results() 
		: mobility_scale()
		, pin_scale()
		, king_attack_scale()
		, unstoppable_pawn_scale()
		, hanging_piece_scale()
		, center_control_scale()
		, connected_rooks_scale()
		, tropism_scale()
	{
		for( int c = 0; c < 2; ++c ) {
			king_pos[c] = 0;
			king_vicinity[c] = 0;

			for( int pi = 0; pi < 7; ++pi ) {
				attacks[c][pi] = 0;
			}

			count_king_attackers[c] = 0;
			king_attacker_sum[c] = 0;

			mobility[c] = 0;
			king_attack[c] = 0;
			pin[c] = 0;
			rooks_on_open_file_bonus[c] = 0;
			tropism[c] = 0;
			center_control[c] = 0;
			rooks_connected[c] = 0;
			hanging[c] = 0;
			unstoppable_pawns[c] = 0;
		}
	}

	uint64_t king_pos[2];
	uint64_t king_vicinity[2];

	uint64_t attacks[2][7];

	short count_king_attackers[2];
	short king_attacker_sum[2];

	// End results
	short mobility[2];
	short king_attack[2];
	short pin[2];
	short rooks_on_open_file_bonus[2];
	short tropism[2];
	short center_control[2];
	short rooks_connected[2];
	short hanging[2];
	short unstoppable_pawns[2];

	// Phase scales
	short mobility_scale;
	short pin_scale;
	short king_attack_scale;
	short unstoppable_pawn_scale;
	short hanging_piece_scale;
	short center_control_scale;
	short connected_rooks_scale;
	short tropism_scale;
};


inline static void evaluate_tropism( position const& p, color::type c, eval_results& results )
{
	uint64_t pieces = p.bitboards[c].b[bb_type::all_pieces];

	while( pieces ) {
		uint64_t piece = bitscan_unset( pieces );
		results.tropism[c] += proximity[piece][results.king_pos[1-c]];
	}
}


inline static void evaluate_pawns_mobility( position const& p, color::type c, eval_results& results )
{
	uint64_t pawns = p.bitboards[c].b[bb_type::pawns];

	while( pawns ) {
		uint64_t pawn = bitscan_unset( pawns );

		uint64_t pc = pawn_control[c][pawn];

		results.attacks[c][pieces::pawn] |= pc;

		pc &= ~p.bitboards[c].b[bb_type::all_pieces];

		if( pc & results.king_vicinity[1-c] ) {
			++results.count_king_attackers[c];
			results.king_attacker_sum[c] += eval_values.king_attack_by_piece[pieces::pawn];
		}

		results.center_control[c] += static_cast<short>(popcount( pc & center_squares ) );
	}
}


inline static void evaluate_knights_mobility( position const& p, color::type c, eval_results& results )
{
	uint64_t knights = p.bitboards[c].b[bb_type::knights];

	while( knights ) {
		uint64_t knight = bitscan_unset( knights );

		uint64_t moves = possible_knight_moves[knight];

		results.attacks[c][pieces::knight] |= moves;

		if( moves & results.king_vicinity[1-c] ) {
			++results.count_king_attackers[c];
			results.king_attacker_sum[c] += eval_values.king_attack_by_piece[pieces::knight];
		}

		moves &= ~p.bitboards[c].b[bb_type::all_pieces];

		results.center_control[c] += static_cast<short>(popcount( moves & center_squares ) );

		results.mobility[c] += eval_values.mobility_knight[popcount(moves)];
	}

}


inline static void evaluate_bishop_mobility( position const& p, color::type c, uint64_t bishop, eval_results& results )
{
	uint64_t const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];

	uint64_t moves = bishop_magic( bishop, all_blockers );

	if( moves & results.king_vicinity[1-c] ) {
		++results.count_king_attackers[c];
		results.king_attacker_sum[c] += eval_values.king_attack_by_piece[pieces::bishop];
	}

	results.attacks[c][pieces::bishop] |= moves;

	moves &= ~p.bitboards[c].b[bb_type::all_pieces];

	results.center_control[c] += static_cast<short>(popcount( moves & center_squares ) );

	results.mobility[c] += eval_values.mobility_bishop[popcount(moves)];
}


inline static void evaluate_bishop_pin( position const& p, color::type c, uint64_t bishop, eval_results& results )
{
	uint64_t own_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawns] | p.bitboards[c].b[bb_type::king];
	uint64_t unblocked_moves = bishop_magic( bishop, own_blockers );

	uint64_t pinned_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];
	uint64_t blocked_moves = bishop_magic( bishop, pinned_blockers );

	// Absolute pin
	if( (unblocked_moves ^ blocked_moves) & p.bitboards[1-c].b[bb_type::king] ) {
		results.pin[c] += eval_values.pin_absolute_bishop;
	}
}


inline static void evaluate_bishops_mobility( position const& p, color::type c, eval_results& results )
{
	uint64_t bishops = p.bitboards[c].b[bb_type::bishops];

	while( bishops ) {
		uint64_t bishop = bitscan_unset( bishops );

		evaluate_bishop_mobility( p, c, bishop, results );
		evaluate_bishop_pin( p, c, bishop, results );
	}
}


inline static void evaluate_rook_mobility( position const& p, color::type c, uint64_t rook, eval_results& results )
{
	uint64_t const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];

	uint64_t moves = rook_magic( rook, all_blockers );

	if( moves & results.king_vicinity[1-c] ) {
		++results.count_king_attackers[c];
		results.king_attacker_sum[c] += eval_values.king_attack_by_piece[pieces::rook];
	}

	results.attacks[c][pieces::rook] |= moves;

	if( moves & p.bitboards[c].b[bb_type::rooks] ) {
		results.rooks_connected[c] = 2;
	}
	moves &= ~p.bitboards[c].b[bb_type::all_pieces];

	results.center_control[c] += static_cast<short>(popcount( moves & center_squares ) );

	results.mobility[c] += eval_values.mobility_rook[popcount(moves)];
}


inline static void evaluate_rook_pin( position const& p, color::type c, uint64_t rook, eval_results& results )
{
	uint64_t own_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawns] | p.bitboards[c].b[bb_type::king];
	uint64_t unblocked_moves = rook_magic( rook, own_blockers );

	uint64_t pinned_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];
	uint64_t blocked_moves = rook_magic( rook, pinned_blockers );

	// Absolute pin
	if( (unblocked_moves ^ blocked_moves) & p.bitboards[1-c].b[bb_type::king] ) {
		results.pin[c] += eval_values.pin_absolute_rook;
	}
}


inline static void evaluate_rook_on_open_file( position const& p, color::type c, uint64_t rook, eval_results& results )
{
	uint64_t file = 0x0101010101010101ull << (rook % 8);
	if( !(p.bitboards[c].b[bb_type::pawns] & file) ) {
		if( p.bitboards[1-c].b[bb_type::pawns] & file ) {
			results.rooks_on_open_file_bonus[c] += static_cast<short>(popcount(p.bitboards[c].b[bb_type::pawns])) * 3;
		}
		else {
			results.rooks_on_open_file_bonus[c] += static_cast<short>(popcount(p.bitboards[c].b[bb_type::pawns])) * 6;
		}
	}
}


inline static void evaluate_rooks_mobility( position const& p, color::type c, eval_results& results )
{
	uint64_t rooks = p.bitboards[c].b[bb_type::rooks];

	while( rooks ) {
		uint64_t rook = bitscan_unset( rooks );

		evaluate_rook_mobility( p, c, rook, results);
		evaluate_rook_pin( p, c, rook, results );
		evaluate_rook_on_open_file( p, c, rook, results );
	}
}


inline static void evaluate_queen_mobility( position const& p, color::type c, uint64_t queen, eval_results& results )
{
	uint64_t const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];

	uint64_t moves = bishop_magic( queen, all_blockers ) | rook_magic( queen, all_blockers );

	if( moves & results.king_vicinity[1-c] ) {
		++results.count_king_attackers[c];
		results.king_attacker_sum[c] += eval_values.king_attack_by_piece[pieces::queen];
	}

	results.attacks[c][pieces::queen] |= moves;

	moves &= ~p.bitboards[c].b[bb_type::all_pieces];

	results.center_control[c] += static_cast<short>(popcount( moves & center_squares ) );

	results.mobility[c] += eval_values.mobility_queen[popcount(moves)];
}


inline static void evaluate_queen_pin( position const& p, color::type c, uint64_t queen, eval_results& results )
{
	uint64_t own_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawns] | p.bitboards[c].b[bb_type::king];
	uint64_t unblocked_moves = bishop_magic( queen, own_blockers ) | rook_magic( queen, own_blockers );

	uint64_t pinned_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];
	uint64_t blocked_moves = bishop_magic( queen, pinned_blockers ) | rook_magic( queen, pinned_blockers );

	// Absolute pin
	if( (unblocked_moves ^ blocked_moves) & p.bitboards[1-c].b[bb_type::king] ) {
		results.pin[c] += eval_values.pin_absolute_queen;
	}
}


inline static void evaluate_queens_mobility( position const& p, color::type c, eval_results& results )
{
	uint64_t queens = p.bitboards[c].b[bb_type::queens];

	while( queens ) {
		uint64_t queen = bitscan_unset( queens );

		evaluate_queen_mobility( p, c, queen, results );
		evaluate_queen_pin( p, c, queen, results );
	}
}


inline static void evaluate_king_mobility( position const& p, color::type c, eval_results& results )
{
	uint64_t queens = p.bitboards[c].b[bb_type::queens];

	while( queens ) {
		uint64_t queen = bitscan_unset( queens );

		evaluate_queen_mobility( p, c, queen, results );
		evaluate_queen_pin( p, c, queen, results );
	}
}


short advance_bonus[] = { 1, 1, 1, 2, 3, 6 };
void evaluate_unstoppable_pawns( position const& p, color::type c, eval_results& results )
{
	for( int i = 0; i < 2; ++i ) {
		uint64_t passed = (p.bitboards[i].b[bb_type::pawns] & p.pawns.passed);
		uint64_t unstoppable = passed & ~rule_of_the_square[1-i][c][results.king_pos[1-i]];
		//results.unstoppable_pawns[i] = popcount( unstoppable ) * eval_values.rule_of_the_square;

		while( passed ) {
			uint64_t pawn = bitscan_unset( passed );
			uint64_t forward_squares = doubled_pawns[1-i][pawn];

			short advance = i ? (6 - pawn / 8) : (pawn / 8 - 1);

			if( (1ull << pawn) & unstoppable ) {
				results.unstoppable_pawns[i] += eval_values.rule_of_the_square * advance_bonus[advance];
			}
			uint64_t hinderers = (results.attacks[1-i][0] & ~results.attacks[i][0]) | p.bitboards[1-i].b[bb_type::all_pieces];
			if( !(forward_squares & hinderers) ) {
				results.unstoppable_pawns[i] += eval_values.passed_pawn_unhindered * advance_bonus[advance];
			}
		}
	}
}


static void evaluate_king_attack( position const& p, color::type c, color::type to_move, eval_results& results )
{
	// Consider a lone attacker harmless
	if( results.count_king_attackers[c] < 2 ) {
		return;
	}

	short attack = 0;

	// Add all the attackers that can reach close to the king
	attack += results.king_attacker_sum[c];

	// Add all positions where a piece can safely give check
	uint64_t secure_squares = ~(results.attacks[1-c][0] | p.bitboards[c].b[bb_type::all_pieces]); // Don't remove enemy pieces, those we would just capture

	uint64_t knight_checks = possible_knight_moves[ results.king_pos[1-c] ] & secure_squares;
	uint64_t const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];
	uint64_t bishop_checks = bishop_magic( results.king_pos[1-c], all_blockers ) & secure_squares;
	uint64_t rook_checks = rook_magic( results.king_pos[1-c], all_blockers ) & secure_squares;

	attack += popcount( results.attacks[c][pieces::knight] & knight_checks ) * eval_values.king_check_by_piece[pieces::knight];
	attack += popcount( results.attacks[c][pieces::bishop] & bishop_checks ) * eval_values.king_check_by_piece[pieces::bishop];
	attack += popcount( results.attacks[c][pieces::rook] & rook_checks ) * eval_values.king_check_by_piece[pieces::rook];
	attack += popcount( results.attacks[c][pieces::queen] & (bishop_checks|rook_checks) ) * eval_values.king_check_by_piece[pieces::queen];

	// Rooks and queens that are able to move next to a king without getting captured are extremely dangerous.
	uint64_t undefended = results.king_vicinity[1-c] & ~(results.attacks[1-c][pieces::pawn] | results.attacks[1-c][pieces::knight] | results.attacks[1-c][pieces::bishop] | results.attacks[1-c][pieces::rook] | results.attacks[1-c][pieces::queen]);
	uint64_t backed_queen_attacks = results.attacks[c][pieces::queen] & (results.attacks[c][pieces::pawn] | results.attacks[c][pieces::knight] | results.attacks[c][pieces::bishop] | results.attacks[c][pieces::rook]);
	uint64_t backed_rook_attacks = results.attacks[c][pieces::rook] & (results.attacks[c][pieces::pawn] | results.attacks[c][pieces::knight] | results.attacks[c][pieces::bishop] | results.attacks[c][pieces::queen]);

	uint64_t king_melee_attack_by_queen = undefended & backed_queen_attacks & ~p.bitboards[c].b[bb_type::all_pieces];
	uint64_t king_melee_attack_by_rook = undefended & backed_rook_attacks & ~p.bitboards[c].b[bb_type::all_pieces];

	short initiative = (to_move == c) ? 2 : 1;
	attack += popcount( king_melee_attack_by_queen ) * eval_values.king_melee_attack_by_queen * initiative;
	attack += popcount( king_melee_attack_by_rook ) * eval_values.king_melee_attack_by_rook * initiative;

	results.king_attack[c] = eval_values.king_attack[ (std::min)( short(150), attack ) ];
}


static void do_evaluate_mobility( position const& p, color::type to_move, eval_results& results )
{
	results.king_pos[0] = bitscan( p.bitboards[0].b[bb_type::king] );
	results.king_pos[1] = bitscan( p.bitboards[1].b[bb_type::king] );
	results.king_vicinity[0] = possible_king_moves[results.king_pos[0]];
	results.king_vicinity[1] = possible_king_moves[results.king_pos[1]];
	
	for( unsigned int c = 0; c < 2; ++c ) {
		evaluate_tropism( p, static_cast<color::type>(c), results );
		evaluate_pawns_mobility( p, static_cast<color::type>(c), results );
		evaluate_knights_mobility( p, static_cast<color::type>(c), results );
		evaluate_bishops_mobility( p, static_cast<color::type>(c), results );
		evaluate_rooks_mobility( p, static_cast<color::type>(c), results );
		evaluate_queens_mobility( p, static_cast<color::type>(c), results );
	
		//Piece-square tables already contain this
		//results.center_control += static_cast<short>(popcount(p.bitboards[c].b[bb_type::all_pieces] & center_squares));

		results.attacks[c][pieces::none] =
				results.attacks[c][pieces::pawn] |
				results.attacks[c][pieces::knight] |
				results.attacks[c][pieces::bishop] |
				results.attacks[c][pieces::rook] |
				results.attacks[c][pieces::queen] |
				results.king_vicinity[c];
	}

	for( unsigned int c = 0; c < 2; ++c ) {
		uint64_t undefended = results.attacks[c][pieces::none] & ~results.attacks[1-c][pieces::none];
		for( unsigned int piece = 1; piece < 6; ++piece ) {
			results.hanging[c] += popcount( p.bitboards[c].b[piece] & undefended ) * eval_values.hanging_piece[piece];
		}

		evaluate_king_attack( p, static_cast<color::type>(c), to_move, results );
	}

	evaluate_unstoppable_pawns( p, to_move, results );
	results.mobility_scale = phase_scale( p.material, eval_values.mobility_scale[0], eval_values.mobility_scale[1]);
	results.pin_scale = phase_scale( p.material, eval_values.pin_scale[0], eval_values.pin_scale[1]);
	results.king_attack_scale = phase_scale( p.material, eval_values.king_attack_scale[0], eval_values.king_attack_scale[1]);
	results.unstoppable_pawn_scale = phase_scale( p.material, eval_values.unstoppable_pawn_scale[0], eval_values.unstoppable_pawn_scale[1]);
	results.hanging_piece_scale = phase_scale( p.material, eval_values.hanging_piece_scale[0], eval_values.hanging_piece_scale[1]);
	results.center_control_scale = phase_scale( p.material, eval_values.center_control_scale[0], eval_values.center_control_scale[1]);
	results.connected_rooks_scale = phase_scale( p.material, eval_values.connected_rooks_scale[0], eval_values.connected_rooks_scale[1] );
	results.tropism_scale = phase_scale( p.material, eval_values.tropism_scale[0], eval_values.tropism_scale[1] );
}
}

short evaluate_mobility( position const& p, color::type c )
{
	eval_results results;
	do_evaluate_mobility( p, c, results );

	return
		((results.mobility[c] - results.mobility[1-c]) * results.mobility_scale) / 20 +
		((results.pin[c] - results.pin[1-c]) * results.pin_scale) / 20 +
		((results.rooks_on_open_file_bonus[c] - results.rooks_on_open_file_bonus[1-c]) * eval_values.rooks_on_open_file_scale) / 20 +
		((results.tropism[c] - results.tropism[1-c]) * results.tropism_scale) / 20 +
		((results.king_attack[c] - results.king_attack[1-c]) * results.king_attack_scale) / 20 +
		((results.center_control[c] - results.center_control[1-c]) * results.center_control_scale) / 20 +
		((results.rooks_connected[c] - results.rooks_connected[1-c]) * results.connected_rooks_scale) / 20 +
		((results.hanging[c] - results.hanging[1-c]) * results.hanging_piece_scale) / 20 +
		((results.unstoppable_pawns[c] - results.unstoppable_pawns[1-c]) * results.unstoppable_pawn_scale) / 20 +
		0;
}

namespace {
static std::string explain( const char* name, short data ) {
	std::stringstream ss;
	ss << std::setw(17) << name << ": " << std::setw(5) << data << std::endl;
	return ss.str();
}
}


std::string explain_eval( position const& p, color::type c )
{
	eval_results results;
	do_evaluate_mobility( p, c, results );

	std::stringstream ss;
	ss << explain( "Material", p.material[c] - p.material[1-c] );
	ss << explain( "Mobility",
		((results.mobility[c] - results.mobility[1-c]) * results.mobility_scale) / 20 );
	ss << explain( "Absolute pins",
		((results.pin[c] - results.pin[1-c]) * results.pin_scale) / 20 );
	ss << explain( "Rook on open file",
		((results.rooks_on_open_file_bonus[c] - results.rooks_on_open_file_bonus[1-c]) * eval_values.rooks_on_open_file_scale) / 20 );
	ss << explain( "King tropism",
		((results.tropism[c] - results.tropism[1-c]) * results.tropism_scale) / 20 );
	ss << explain( "King attack",
		((results.king_attack[c] - results.king_attack[1-c]) * results.king_attack_scale) / 20 );
	ss << explain( "Center control",
		((results.center_control[c] - results.center_control[1-c]) * results.center_control_scale) / 20 );
	ss << explain( "Connected rooks",
		((results.rooks_connected[c] - results.rooks_connected[1-c]) * results.connected_rooks_scale) / 20 );
	ss << explain( "Hanging pieces",
		((results.hanging[c] - results.hanging[1-c]) * results.hanging_piece_scale) / 20 );
	ss << explain( "Unstoppable pawns",
		((results.unstoppable_pawns[c] - results.unstoppable_pawns[1-c]) * results.unstoppable_pawn_scale) / 20 );

	return ss.str();
}
