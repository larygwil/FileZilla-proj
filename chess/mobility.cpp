#include "mobility.hpp"
#include "mobility_data.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

extern unsigned long long const possible_knight_moves[64];
extern unsigned long long const possible_king_moves[64];

unsigned long long const center_squares = 0x00003c3c3c3c0000ull;

namespace {

namespace pin_values {
enum type {
	absolute_bishop = 35,
	absolute_rook = 30,
	absolute_queen = 25
};
}

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

// Using the XRAYS pattern, own pieces do not stop mobility.
// We do stop at own pawns and king though.
// Naturally, enemy pieces also block us.

// Punish bad mobility, reward great mobility
short knight_mobility_scale[] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8
};
short bishop_mobility_scale[] = {
	-1, 0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
};
short rook_mobility_scale[] = {
	-1, 0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
};


inline static void evaluate_tropism( position const& p, color::type c, bitboard const* bitboards, eval_data const& data, eval_results& results )
{
	unsigned long long pieces = bitboards[c].all_pieces;

	unsigned long long piece;
	while( pieces ) {
		bitscan( pieces, piece );
		pieces ^= 1ull << piece;

		results.tropism += proximity[piece][data.other_king_pos];
	}
}


inline static void evaluate_pawns_mobility( position const& p, color::type c, bitboard const* bitboards, eval_data const& data, eval_results& results )
{
	unsigned long long pawns = bitboards[c].pawns;

	unsigned long long pawn;
	while( pawns ) {
		bitscan( pawns, pawn );
		pawns ^= 1ull << pawn;

		unsigned long long pc = pawn_control[c][pawn];
		results.king_attack += static_cast<short>(popcount( pc & data.king_vicinity ) );
		results.center_control += static_cast<short>(popcount( pc & center_squares ) );
	}
}


inline static void evaluate_knights_mobility( position const& p, color::type c, bitboard const* bitboards, eval_data const& data, eval_results& results )
{
	unsigned long long knights = bitboards[c].knights;

	unsigned long long knight;
	while( knights ) {
		bitscan( knights, knight );
		knights ^= 1ull << knight;

		unsigned long long moves = possible_knight_moves[knight];
		moves &= ~bitboards[c].all_pieces;

		results.king_attack += static_cast<short>(popcount( moves & data.king_vicinity ));

		moves &= ~bitboards[1-c].pawn_control;

		short kev = static_cast<short>(popcount(moves));

		results.center_control += static_cast<short>(popcount( moves & center_squares ) );

		results.mobility += knight_mobility_scale[kev];
	}

}


inline static void evaluate_bishop_mobility( position const& p, color::type c, bitboard const* bitboards, unsigned long long bishop, unsigned long long moves, eval_data const& data, eval_results& results )
{
	unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns | bitboards[c].king;
	blockers &= moves;

	moves &= ~bitboards[c].all_pieces;
	moves &= ~bitboards[1-c].pawn_control;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[bishop][blocker];
		moves &= ~mobility_block[bishop][blocker];
	}

	short bev = static_cast<short>(popcount(moves));

	results.king_attack += static_cast<short>(popcount( moves & data.king_vicinity ));

	results.center_control += static_cast<short>(popcount( moves & center_squares ) );

	results.mobility += bishop_mobility_scale[bev];
}


inline static void evaluate_bishop_pin( position const& p, color::type c, bitboard const* bitboards, unsigned long long bishop, unsigned long long moves, eval_results& results )
{
	unsigned long long blockers = bitboards[c].all_pieces | bitboards[1-c].pawns;
	blockers &= moves;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[bishop][blocker];
		moves &= ~mobility_block[bishop][blocker];
	}

	unsigned long long unblocked_moves = moves;

	blockers = bitboards[1-c].all_pieces;
	blockers &= moves;

	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[bishop][blocker];
		moves &= ~mobility_block[bishop][blocker];
	}

	// Absolute pin
	if( (moves ^ unblocked_moves) & bitboards[1-c].king ) {
		results.pin += pin_values::absolute_bishop;
	}
}


inline static void evaluate_bishops_mobility( position const& p, color::type c, bitboard const* bitboards, eval_data const& data, eval_results& results )
{
	unsigned long long bishops = bitboards[c].bishops;

	unsigned long long bishop;
	while( bishops ) {
		bitscan( bishops, bishop );
		bishops ^= 1ull << bishop;

		unsigned long long moves = visibility_bishop[bishop];
		evaluate_bishop_mobility( p, c, bitboards, bishop, moves, data, results );
		evaluate_bishop_pin( p, c, bitboards, bishop, moves, results );
	}
}


inline static void evaluate_rook_mobility( position const& p, color::type c, bitboard const* bitboards, unsigned long long rook, unsigned long long moves, eval_data const& data, eval_results& results )
{
	unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns | bitboards[c].king;
	blockers &= moves;

	moves &= ~bitboards[c].all_pieces;
	moves &= ~bitboards[1-c].pawn_control;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[rook][blocker];
		moves &= ~mobility_block[rook][blocker];
	}

	results.center_control += static_cast<short>(popcount( moves & center_squares ) );

	results.king_attack += static_cast<short>(popcount( moves & data.king_vicinity ));

	unsigned long long vertical = moves & (0x0101010101010101ull << (rook % 8));
	unsigned long long horizontal = moves & ~vertical;

	// Rooks dig verticals.
	short rev = static_cast<short>(popcount(horizontal)) + 2 * static_cast<short>(popcount(vertical));

	// Punish bad mobility
	results.mobility += rook_mobility_scale[rev];

}


inline static void evaluate_rook_pin( position const& p, color::type c, bitboard const* bitboards, unsigned long long rook, unsigned long long moves, eval_results& results )
{
	unsigned long long blockers = bitboards[c].all_pieces | bitboards[1-c].pawns;
	blockers &= moves;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[rook][blocker];
		moves &= ~mobility_block[rook][blocker];
	}

	unsigned long long unblocked_moves = moves;

	blockers = bitboards[1-c].all_pieces;
	blockers &= moves;

	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[rook][blocker];
		moves &= ~mobility_block[rook][blocker];
	}

	// Absolute pin
	if( (moves ^ unblocked_moves) & bitboards[1-c].king ) {
		results.pin += pin_values::absolute_rook;
	}
}


inline static void evaluate_rook_on_open_file( position const& p, color::type c, bitboard const* bitboards, unsigned long long rook, eval_results& results )
{
	unsigned long long file = 0x0101010101010101ull << (rook % 8);
	if( !(bitboards[c].pawns & file) ) {
		if( bitboards[1-c].pawns & file ) {
			results.rooks_on_open_file_bonus += static_cast<short>(popcount(bitboards[c].pawns)) * 3;
		}
		else {
			results.rooks_on_open_file_bonus += static_cast<short>(popcount(bitboards[c].pawns)) * 6;
		}
	}
}


inline static void evaluate_rooks_mobility( position const& p, color::type c, bitboard const* bitboards, eval_data const& data, eval_results& results )
{
	unsigned long long rooks = bitboards[c].rooks;

	unsigned long long rook;
	while( rooks ) {
		bitscan( rooks, rook );
		rooks ^= 1ull << rook;

		unsigned long long moves = visibility_rook[rook];
		evaluate_rook_mobility( p, c, bitboards, rook, moves, data, results);
		evaluate_rook_pin( p, c, bitboards, rook, moves, results );

		evaluate_rook_on_open_file( p, c, bitboards, rook, results );
	}
}


inline static void evaluate_queen_mobility( position const& p, color::type c, bitboard const* bitboards, unsigned long long queen, unsigned long long moves, eval_data const& data, eval_results& results )
{
	unsigned long long blockers = bitboards[1-c].all_pieces | bitboards[c].pawns | bitboards[c].king;
	blockers &= moves;

	moves &= ~bitboards[c].all_pieces;
	moves &= ~bitboards[1-c].pawn_control;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[queen][blocker];
		moves &= ~mobility_block[queen][blocker];
	}

	results.king_attack += static_cast<short>(popcount( moves & data.king_vicinity ));

	results.center_control += static_cast<short>(popcount( moves & center_squares ) );

	// Queen is just too mobile, can get anywhere quickly. Just punish very low mobility.
	short qev = static_cast<short>(popcount(moves));
	results.mobility += (qev > 5) ? 5 : qev;
}


inline static void evaluate_queen_pin( position const& p, color::type c, bitboard const* bitboards, unsigned long long queen, unsigned long long moves, eval_results& results )
{
	unsigned long long blockers = bitboards[c].all_pieces | bitboards[1-c].pawns;
	blockers &= moves;

	unsigned long long blocker;
	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[queen][blocker];
		moves &= ~mobility_block[queen][blocker];
	}

	unsigned long long unblocked_moves = moves;

	blockers = bitboards[1-c].all_pieces;
	blockers &= moves;

	while( blockers ) {
		bitscan( blockers, blocker );
		blockers ^= 1ull << blocker;

		blockers &= ~mobility_block[queen][blocker];
		moves &= ~mobility_block[queen][blocker];
	}

	// Absolute pin
	if( (moves ^ unblocked_moves) & bitboards[1-c].king ) {
		results.pin += pin_values::absolute_queen;
	}
}


inline static void evaluate_queens_mobility( position const& p, color::type c, bitboard const* bitboards, eval_data const& data, eval_results& results )
{
	unsigned long long queens = bitboards[c].queens;

	unsigned long long queen;
	while( queens ) {
		bitscan( queens, queen );
		queens ^= 1ull << queen;

		unsigned long long moves = visibility_bishop[queen] | visibility_rook[queen];
		evaluate_queen_mobility( p, c, bitboards, queen, moves, data, results );
		evaluate_queen_pin( p, c, bitboards, queen, moves, results );
	}
}

static void do_evaluate_mobility( position const& p, color::type c, bitboard const* bitboards, eval_results& results_self, eval_results& results_other )
{
	eval_data data_self;
	
	bitscan( bitboards[1-c].king, data_self.other_king_pos );
	data_self.king_vicinity = possible_king_moves[data_self.other_king_pos];
	
	evaluate_tropism( p, c, bitboards, data_self, results_self );
	evaluate_pawns_mobility( p, c, bitboards, data_self, results_self );
	evaluate_knights_mobility( p, c, bitboards, data_self, results_self );
	evaluate_bishops_mobility( p, c, bitboards, data_self, results_self );
	evaluate_rooks_mobility( p, c, bitboards, data_self, results_self );
	evaluate_queens_mobility( p, c, bitboards, data_self, results_self );
	results_self.center_control += static_cast<short>(popcount(bitboards[c].all_pieces & center_squares));

	eval_data data_other;
	
	bitscan( bitboards[c].king, data_other.other_king_pos );
	data_other.king_vicinity = possible_king_moves[data_other.other_king_pos];
	
	evaluate_tropism( p, static_cast<color::type>(1-c), bitboards, data_other, results_other );
	evaluate_pawns_mobility( p, static_cast<color::type>(1-c), bitboards, data_other, results_other );
	evaluate_knights_mobility( p, static_cast<color::type>(1-c), bitboards, data_other, results_other );
	evaluate_bishops_mobility( p, static_cast<color::type>(1-c), bitboards, data_other, results_other );
	evaluate_rooks_mobility( p, static_cast<color::type>(1-c), bitboards, data_other, results_other );
	evaluate_queens_mobility( p, static_cast<color::type>(1-c), bitboards, data_other, results_other );
	results_other.center_control += static_cast<short>(popcount(bitboards[1-c].all_pieces & center_squares));
}
}

short evaluate_mobility( position const& p, color::type c, bitboard const* bitboards )
{
	eval_results results_self;
	eval_results results_other;
	do_evaluate_mobility( p, c, bitboards, results_self, results_other );

	return
		results_self.mobility - results_other.mobility +
		results_self.pin - results_other.pin +
		results_self.rooks_on_open_file_bonus - results_other.rooks_on_open_file_bonus +
		results_self.tropism - results_other.tropism +
		results_self.king_attack - results_other.king_attack +
		results_self.center_control - results_other.center_control +
		0;
}

namespace {
static std::string explain( const char* name, short left, short right ) {
	std::stringstream ss;
	ss << std::setw(15) << name << ": " << std::setw(5) << left - right << " = " << std::setw(5) << left << " - " << std::setw(5) << right << std::endl;
	return ss.str();
}
}


std::string explain_eval( position const& p, color::type c, bitboard const* bitboards )
{
	eval_results results_self;
	eval_results results_other;
	do_evaluate_mobility( p, c, bitboards, results_self, results_other );

	std::stringstream ss;
	ss << explain( "Material", p.material[c], p.material[1-c] );
	ss << explain( "Mobility", results_self.mobility, results_other.mobility );
	ss << explain( "Absolute pins", results_self.pin, results_other.pin );
	ss << explain( "Rook on open file bonus", results_self.rooks_on_open_file_bonus, results_other.rooks_on_open_file_bonus );
	ss << explain( "King attack", results_self.king_attack, results_other.king_attack );
	ss << explain( "King tropism", results_self.tropism, results_other.tropism );
	ss << explain( "Center control", results_self.center_control, results_other.center_control );

	return ss.str();
}