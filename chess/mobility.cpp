#include "mobility.hpp"
#include "tables.hpp"
#include "eval.hpp"
#include "magic.hpp"
#include "sliding_piece_attacks.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

unsigned long long const center_squares = 0x00003c3c3c3c0000ull;

namespace {

struct eval_data {
	unsigned long long other_king_pos;
	unsigned long long king_vicinity;
};

struct eval_results {
	eval_results() 
		: mobility()
		, king_attack()
		, pin()
		, rooks_on_open_file_bonus()
		, tropism()
		, center_control()
	{
	}

	// Results
	short mobility;
	short king_attack;
	short pin;
	short rooks_on_open_file_bonus;
	short tropism;
	short center_control;
};

//#define XRAYS 1
// If using the XRAYS pattern, own pieces do not stop mobility.

// We do stop at own pawns and king though.
// Naturally, enemy pieces also block us.

// Punish bad mobility, reward great mobility
short knight_mobility_scale[] = {
	-7, -3, 0, 3, 4, 5, 6, 8, 10
};
short bishop_mobility_scale[] = {
	-5, -2, 0, 1, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
};
short rook_mobility_scale[] = {
	-5, -2, 0, 1, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
};


inline static void evaluate_tropism( position const& p, color::type c, eval_data const& data, eval_results& results )
{
	unsigned long long pieces = p.bitboards[c].b[bb_type::all_pieces];

	unsigned long long piece;
	while( pieces ) {
		bitscan( pieces, piece );
		pieces &= pieces - 1;

		results.tropism += proximity[piece][data.other_king_pos];
	}
}


inline static void evaluate_pawns_mobility( position const& p, color::type c, eval_data const& data, eval_results& results )
{
	unsigned long long pawns = p.bitboards[c].b[bb_type::pawns];

	unsigned long long pawn;
	while( pawns ) {
		bitscan( pawns, pawn );
		pawns &= pawns - 1;

		unsigned long long pc = pawn_control[c][pawn];
		results.king_attack += static_cast<short>(popcount( pc & data.king_vicinity ) );
		results.center_control += static_cast<short>(popcount( pc & center_squares ) );
	}
}


inline static void evaluate_knights_mobility( position const& p, color::type c, eval_data const& data, eval_results& results )
{
	unsigned long long knights = p.bitboards[c].b[bb_type::knights];

	unsigned long long knight;
	while( knights ) {
		bitscan( knights, knight );
		knights &= knights - 1;

		unsigned long long moves = possible_knight_moves[knight];
		moves &= ~p.bitboards[c].b[bb_type::all_pieces];

		results.king_attack += static_cast<short>(popcount( moves & data.king_vicinity ));

		moves &= ~p.bitboards[1-c].b[bb_type::pawn_control];

		short kev = static_cast<short>(popcount(moves));

		results.center_control += static_cast<short>(popcount( moves & center_squares ) );

		results.mobility += knight_mobility_scale[kev];
	}

}


inline static void evaluate_bishop_mobility( position const& p, color::type c, unsigned long long bishop, eval_data const& data, eval_results& results )
{
#if XRAYS
	unsigned long long const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::pawns] | p.bitboards[c].b[bb_type::king];
#else
	unsigned long long const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];
#endif

	unsigned long long moves = bishop_magic( bishop, all_blockers );
	moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawn_control] );

	short bev = static_cast<short>(popcount(moves));

	results.king_attack += static_cast<short>(popcount( moves & data.king_vicinity ));

	results.center_control += static_cast<short>(popcount( moves & center_squares ) );

	results.mobility += bishop_mobility_scale[bev];
}


inline static void evaluate_bishop_pin( position const& p, color::type c, unsigned long long bishop, eval_results& results )
{
	unsigned long long own_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawns] | p.bitboards[c].b[bb_type::king];
	unsigned long long unblocked_moves = bishop_magic( bishop, own_blockers );

	unsigned long long pinned_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];
	unsigned long long blocked_moves = bishop_magic( bishop, pinned_blockers );

	// Absolute pin
	if( (unblocked_moves ^ blocked_moves) & p.bitboards[1-c].b[bb_type::king] ) {
		results.pin += eval_values.pin_absolute_bishop;
	}
}


inline static void evaluate_bishops_mobility( position const& p, color::type c, eval_data const& data, eval_results& results )
{
	unsigned long long bishops = p.bitboards[c].b[bb_type::bishops];

	unsigned long long bishop;
	while( bishops ) {
		bitscan( bishops, bishop );
		bishops &= bishops - 1;

		evaluate_bishop_mobility( p, c, bishop, data, results );
		evaluate_bishop_pin( p, c, bishop, results );
	}
}


inline static void evaluate_rook_mobility( position const& p, color::type c, unsigned long long rook, eval_data const& data, eval_results& results )
{
#if XRAYS
	unsigned long long const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::pawns] | p.bitboards[c].b[bb_type::king];
#else
	unsigned long long const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];
#endif

	unsigned long long moves = rook_magic( rook, all_blockers );
	moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawn_control] );

	results.center_control += static_cast<short>(popcount( moves & center_squares ) );

	results.king_attack += static_cast<short>(popcount( moves & data.king_vicinity ));

	unsigned long long vertical = moves & (0x0101010101010101ull << (rook % 8));
	unsigned long long horizontal = moves & ~vertical;

	// Rooks dig verticals.
	short rev = static_cast<short>(popcount(horizontal)) + 2 * static_cast<short>(popcount(vertical));

	// Punish bad mobility
	results.mobility += rook_mobility_scale[rev];
}


inline static void evaluate_rook_pin( position const& p, color::type c, unsigned long long rook, eval_results& results )
{
	unsigned long long own_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawns] | p.bitboards[c].b[bb_type::king];
	unsigned long long unblocked_moves = rook_magic( rook, own_blockers );

	unsigned long long pinned_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];
	unsigned long long blocked_moves = rook_magic( rook, pinned_blockers );

	// Absolute pin
	if( (unblocked_moves ^ blocked_moves) & p.bitboards[1-c].b[bb_type::king] ) {
		results.pin += eval_values.pin_absolute_rook;
	}
}


inline static void evaluate_rook_on_open_file( position const& p, color::type c, unsigned long long rook, eval_results& results )
{
	unsigned long long file = 0x0101010101010101ull << (rook % 8);
	if( !(p.bitboards[c].b[bb_type::pawns] & file) ) {
		if( p.bitboards[1-c].b[bb_type::pawns] & file ) {
			results.rooks_on_open_file_bonus += static_cast<short>(popcount(p.bitboards[c].b[bb_type::pawns])) * 3;
		}
		else {
			results.rooks_on_open_file_bonus += static_cast<short>(popcount(p.bitboards[c].b[bb_type::pawns])) * 6;
		}
	}
}


inline static void evaluate_rooks_mobility( position const& p, color::type c, eval_data const& data, eval_results& results )
{
	unsigned long long rooks = p.bitboards[c].b[bb_type::rooks];

	unsigned long long rook;
	while( rooks ) {
		bitscan( rooks, rook );
		rooks &= rooks - 1;

		evaluate_rook_mobility( p, c, rook, data, results);
		evaluate_rook_pin( p, c, rook, results );
		evaluate_rook_on_open_file( p, c, rook, results );
	}
}


inline static void evaluate_queen_mobility( position const& p, color::type c, unsigned long long queen, eval_data const& data, eval_results& results )
{
#if XRAYS
	unsigned long long const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::pawns] | p.bitboards[c].b[bb_type::king];
#else
	unsigned long long const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];
#endif

	unsigned long long moves = bishop_magic( queen, all_blockers ) | rook_magic( queen, all_blockers );
	moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawn_control] );

	results.king_attack += static_cast<short>(popcount( moves & data.king_vicinity ));

	results.center_control += static_cast<short>(popcount( moves & center_squares ) );

	// Queen is just too mobile, can get anywhere quickly. Just punish very low mobility.
	short qev = static_cast<short>(popcount(moves));
	results.mobility += (qev > 5) ? 5 : qev;
}


inline static void evaluate_queen_pin( position const& p, color::type c, unsigned long long queen, eval_results& results )
{
	unsigned long long own_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawns] | p.bitboards[c].b[bb_type::king];
	unsigned long long unblocked_moves = bishop_magic( queen, own_blockers ) | rook_magic( queen, own_blockers );

	unsigned long long pinned_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];
	unsigned long long blocked_moves = bishop_magic( queen, pinned_blockers ) | rook_magic( queen, pinned_blockers );

	// Absolute pin
	if( (unblocked_moves ^ blocked_moves) & p.bitboards[1-c].b[bb_type::king] ) {
		results.pin += eval_values.pin_absolute_queen;
	}
}


inline static void evaluate_queens_mobility( position const& p, color::type c, eval_data const& data, eval_results& results )
{
	unsigned long long queens = p.bitboards[c].b[bb_type::queens];

	unsigned long long queen;
	while( queens ) {
		bitscan( queens, queen );
		queens &= queens - 1;

		evaluate_queen_mobility( p, c, queen, data, results );
		evaluate_queen_pin( p, c, queen, results );
	}
}

static void do_evaluate_mobility( position const& p, color::type c, eval_results& results_self, eval_results& results_other )
{
	eval_data data_self;
	
	{
		unsigned long long kings = p.bitboards[1-c].b[bb_type::king];
		unsigned long long king;
		bitscan( kings, king );
		data_self.other_king_pos = king;
	}
	data_self.king_vicinity = possible_king_moves[data_self.other_king_pos];
	
	evaluate_tropism( p, c, data_self, results_self );
	evaluate_pawns_mobility( p, c, data_self, results_self );
	evaluate_knights_mobility( p, c, data_self, results_self );
	evaluate_bishops_mobility( p, c, data_self, results_self );
	evaluate_rooks_mobility( p, c, data_self, results_self );
	evaluate_queens_mobility( p, c, data_self, results_self );
	
	//Piece-square tables already contain this
	//results_self.center_control += static_cast<short>(popcount(p.bitboards[c].b[bb_type::all_pieces] & center_squares));

	eval_data data_other;
	
	{
		unsigned long long kings = p.bitboards[c].b[bb_type::king];
		unsigned long long king;
		bitscan( kings, king );
		data_other.other_king_pos = king;
	}
	data_other.king_vicinity = possible_king_moves[data_other.other_king_pos];
	
	evaluate_tropism( p, static_cast<color::type>(1-c), data_other, results_other );
	evaluate_pawns_mobility( p, static_cast<color::type>(1-c), data_other, results_other );
	evaluate_knights_mobility( p, static_cast<color::type>(1-c), data_other, results_other );
	evaluate_bishops_mobility( p, static_cast<color::type>(1-c), data_other, results_other );
	evaluate_rooks_mobility( p, static_cast<color::type>(1-c), data_other, results_other );
	evaluate_queens_mobility( p, static_cast<color::type>(1-c), data_other, results_other );

	//Piece-square tables already contain this
	//results_other.center_control += static_cast<short>(popcount(p.bitboards[1-c].b[bb_type::all_pieces] & center_squares));
}
}

short evaluate_mobility( position const& p, color::type c )
{
	eval_results results_self;
	eval_results results_other;
	do_evaluate_mobility( p, c, results_self, results_other );

	return
		((results_self.mobility - results_other.mobility) * eval_values.mobility_multiplicator) / eval_values.mobility_divisor +
		((results_self.pin - results_other.pin) * eval_values.pin_multiplicator) / eval_values.pin_divisor +
		((results_self.rooks_on_open_file_bonus - results_other.rooks_on_open_file_bonus) * eval_values.rooks_on_open_file_multiplicator) / eval_values.rooks_on_open_file_divisor +
		((results_self.tropism - results_other.tropism) * eval_values.tropism_multiplicator) / eval_values.tropism_divisor +
		((results_self.king_attack - results_other.king_attack) * eval_values.king_attack_multiplicator) / eval_values.king_attack_divisor +
		((results_self.center_control - results_other.center_control) * eval_values.center_control_multiplicator) / eval_values.center_control_divisor +
		0;
}

namespace {
static std::string explain( const char* name, short left, short right ) {
	std::stringstream ss;
	ss << std::setw(15) << name << ": " << std::setw(5) << left - right << " = " << std::setw(5) << left << " - " << std::setw(5) << right << std::endl;
	return ss.str();
}
static std::string explain( const char* name, short data ) {
	std::stringstream ss;
	ss << std::setw(15) << name << ": " << std::setw(5) << data << std::endl;
	return ss.str();
}
}


std::string explain_eval( position const& p, color::type c )
{
	eval_results results_self;
	eval_results results_other;
	do_evaluate_mobility( p, c, results_self, results_other );

	std::stringstream ss;
	ss << explain( "Material", p.material[c] - p.material[1-c] );
	ss << explain( "Mobility",
		((results_self.mobility - results_other.mobility) * eval_values.mobility_multiplicator) / eval_values.mobility_divisor );
	ss << explain( "Absolute pins",
		((results_self.pin - results_other.pin) * eval_values.pin_multiplicator) / eval_values.pin_divisor );
	ss << explain( "Rook on open file bonus",
		((results_self.rooks_on_open_file_bonus - results_other.rooks_on_open_file_bonus) * eval_values.rooks_on_open_file_multiplicator) / eval_values.rooks_on_open_file_divisor );
	ss << explain( "King tropism",
		((results_self.tropism - results_other.tropism) * eval_values.tropism_multiplicator) / eval_values.tropism_divisor );
	ss << explain( "Center control",
		((results_self.center_control - results_other.center_control) * eval_values.center_control_multiplicator) / eval_values.center_control_divisor );
	ss << explain( "King attack",
		((results_self.king_attack - results_other.king_attack) * eval_values.king_attack_multiplicator) / eval_values.king_attack_divisor );

	return ss.str();
}
