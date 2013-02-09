#include "eval.hpp"
#include "eval_values.hpp"
#include "endgame.hpp"
#include "assert.hpp"
#include "fen.hpp"
#include "magic.hpp"
#include "pawn_structure_hash_table.hpp"
#include "util.hpp"
#include "tables.hpp"
#include "zobrist.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>

namespace eval_detail {
enum type {
	material,
	imbalance,
	pst,
	pawn_structure,
	pawn_shield,
	passed_pawns,
	king_attack,
	king_tropism,
	mobility,
	center_control,
	absolute_pins,
	defended_by_pawn,
	attacked_pieces,
	hanging_pieces,
	connected_rooks,
	rooks_on_open_file,
	rooks_on_7h_rank,
	drawishness,
	outposts,
	double_bishop,
	side_to_move,
	trapped_rook,

	max
};
}

namespace {

struct eval_results {
	eval_results()
		: passed_pawns()
	{
		for( int c = 0; c < 2; ++c ) {

			for( int pi = 0; pi < 7; ++pi ) {
				attacks[c][pi] = 0;
			}

			count_king_attackers[c] = 0;
			king_attacker_sum[c] = 0;
			pawn_shield[c] = 0;
		}
	}

	uint64_t attacks[2][7];
	short pawn_shield[2];

	short count_king_attackers[2];
	short king_attacker_sum[2];

	uint64_t passed_pawns;

	// End results
	score eval[2];
};

score detailed_results[eval_detail::max][2];

template<bool detail, eval_detail::type t>
inline void add_score( eval_results& results, color::type c, score const& s ) {
	if( detail ) {
		detailed_results[t][c] += s;
	}
	results.eval[c] += s;
}

template<bool detail, eval_detail::type t>
inline void add_score( eval_results& results, score const* s ) {
	if( detail ) {
		detailed_results[t][0] += s[0];
		detailed_results[t][1] += s[1];
	}
	results.eval[0] += s[0];
	results.eval[1] += s[1];
}

}

short evaluate_move( position const& p, move const& m )
{
	pieces::type piece = p.get_piece(m.source());
	score delta = -pst[p.self()][piece][m.source()];

	if( m.castle() ) {
		delta += pst[p.self()][pieces::king][m.target()];

		unsigned char row = p.white() ? 0 : 56;
		if( m.target() % 8 == 6 ) {
			// Kingside
			delta += pst[p.self()][pieces::rook][row + 5] - pst[p.self()][pieces::rook][row + 7];
		}
		else {
			// Queenside
			delta += pst[p.self()][pieces::rook][row + 3] - pst[p.self()][pieces::rook][row];
		}
	}
	else {
		pieces::type captured_piece = p.get_captured_piece( m );
		if( captured_piece != pieces::none ) {
			if( m.enpassant() ) {
				unsigned char ep = (m.target() & 0x7) | (m.source() & 0x38);
				delta += eval_values::material_values[pieces::pawn] + pst[p.other()][pieces::pawn][ep];
			}
			else {
				delta += eval_values::material_values[captured_piece] + pst[p.other()][captured_piece][m.target()];
			}
		}

		if( m.promotion() ) {
			pieces::type promotion_piece = m.promotion_piece();
			delta -= eval_values::material_values[pieces::pawn];
			delta += eval_values::material_values[promotion_piece] + pst[p.self()][promotion_piece][m.target()];
		}
		else {
			delta += pst[p.self()][piece][m.target()];
		}
	}

#if VERIFY_EVAL
	{
		position p2 = p;
		apply_move( p2, m );
		ASSERT( p2.verify() );
		ASSERT( p.base_eval + (p.white() ? delta : -delta) == p2.base_eval );
	}
#endif

	return delta.scale( p.material[0].mg() + p.material[1].mg() );
}


uint64_t const central_squares[2] = { 0x000000003c3c3c00ull, 0x003c3c3c00000000ull };

namespace {


template<bool detail>
inline static void evaluate_pawns_mobility( position const& p, color::type c, eval_results& results )
{
	uint64_t pawns = p.bitboards[c].b[bb_type::pawns];

	while( pawns ) {
		uint64_t pawn = bitscan_unset( pawns );

		add_score<detail, eval_detail::king_tropism>( results, c, eval_values::tropism[pieces::pawn] * proximity[pawn][p.king_pos[1-c]] );

		uint64_t pc = pawn_control[c][pawn];

		pc &= ~p.bitboards[c].b[bb_type::all_pieces];

		if( pc & king_attack_zone[1-c][p.king_pos[1-c]] ) {
			++results.count_king_attackers[c];
			results.king_attacker_sum[c] += eval_values::king_attack_by_piece[pieces::pawn];
		}
	}
}

template<bool detail>
inline static void evaluate_knight_mobility( position const& p, color::type c, uint64_t knight, eval_results& results )
{
	uint64_t moves = possible_knight_moves[knight];

	results.attacks[c][pieces::knight] |= moves;

	if( moves & king_attack_zone[1-c][p.king_pos[1-c]] ) {
		++results.count_king_attackers[c];
		results.king_attacker_sum[c] += eval_values::king_attack_by_piece[pieces::knight];
	}

	moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawn_control]);
	add_score<detail, eval_detail::mobility>( results, c, eval_values::mobility_knight[popcount(moves)] );
}


short outpost_squares[64] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 1, 1, 1, 1, 1, 1, 0,
	0, 1, 1, 1, 1, 1, 1, 0,
	0, 1, 1, 1, 1, 1, 1, 0,
	0, 1, 1, 1, 1, 1, 1, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};


template<bool detail>
inline static void evaluate_knight_outpost( position const& p, color::type c, uint64_t knight, eval_results& results )
{
	if( outpost_squares[knight] && !(p.bitboards[1-c].b[bb_type::pawns] & forward_pawn_attack[c][knight] ) ) {
		// We're in a sunny outpost. Check whether theres a pawn to cover us
		if( (1ull << knight) & p.bitboards[c].b[bb_type::pawn_control] ) {
			add_score<detail, eval_detail::outposts>( results, c, eval_values::knight_outposts[1] );
		}
		else {
			add_score<detail, eval_detail::outposts>( results, c, eval_values::knight_outposts[0] );
		}
	}
}


template<bool detail>
inline static void evaluate_knights( position const& p, color::type c, eval_results& results )
{
	uint64_t knights = p.bitboards[c].b[bb_type::knights];

	while( knights ) {
		uint64_t knight = bitscan_unset( knights );

		add_score<detail, eval_detail::king_tropism>( results, c, eval_values::tropism[pieces::knight] * proximity[knight][p.king_pos[1-c]] );

		evaluate_knight_mobility<detail>( p, c, knight, results );
		evaluate_knight_outpost<detail>( p, c, knight, results );
	}

}


template<bool detail>
inline static void evaluate_bishop_mobility( position const& p, color::type c, uint64_t bishop, eval_results& results )
{
	uint64_t const all_blockers = (p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces]) & ~p.bitboards[c].b[bb_type::queens];

	uint64_t moves = bishop_magic( bishop, all_blockers );

	if( moves & king_attack_zone[1-c][p.king_pos[1-c]] ) {
		++results.count_king_attackers[c];
		results.king_attacker_sum[c] += eval_values::king_attack_by_piece[pieces::bishop];
	}

	results.attacks[c][pieces::bishop] |= moves;

	moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawn_control]);
	add_score<detail, eval_detail::mobility>( results, c, eval_values::mobility_bishop[popcount(moves)] );
}


template<bool detail>
inline static void evaluate_bishop_pin( position const& p, color::type c, uint64_t bishop, eval_results& results )
{
	uint64_t own_blockers = p.bitboards[c].b[bb_type::all_pieces];
	uint64_t unblocked_moves = bishop_magic( bishop, own_blockers );

	if( unblocked_moves & p.bitboards[1-c].b[bb_type::king] ) {

		uint64_t between = between_squares[bishop][p.king_pos[1-c]] & p.bitboards[1-c].b[bb_type::all_pieces];
		if( popcount( between ) == 1 ) {
			pieces::type piece = p.get_piece( bitscan( between ) );
			add_score<detail, eval_detail::absolute_pins>( results, c, eval_values::absolute_pin[ piece ] );
		}
	}
}


template<bool detail>
inline static void evaluate_bishop_outpost( position const& p, color::type c, uint64_t bishop, eval_results& results )
{
	if( outpost_squares[bishop] && !(p.bitboards[1-c].b[bb_type::pawns] & forward_pawn_attack[c][bishop] ) ) {
		// We're in a sunny outpost. Check whether theres a pawn to cover us
		if( (1ull << bishop) & p.bitboards[c].b[bb_type::pawn_control] ) {
			add_score<detail, eval_detail::outposts>( results, c, eval_values::bishop_outposts[1] );
		}
		else {
			add_score<detail, eval_detail::outposts>( results, c, eval_values::bishop_outposts[0] );
		}
	}
}


template<bool detail>
inline static void evaluate_bishops( position const& p, color::type c, eval_results& results )
{
	uint64_t bishops = p.bitboards[c].b[bb_type::bishops];

	while( bishops ) {
		uint64_t bishop = bitscan_unset( bishops );

		add_score<detail, eval_detail::king_tropism>( results, c, eval_values::tropism[pieces::bishop] * proximity[bishop][p.king_pos[1-c]] );

		evaluate_bishop_mobility<detail>( p, c, bishop, results );
		evaluate_bishop_pin<detail>( p, c, bishop, results );
		evaluate_bishop_outpost<detail>( p, c, bishop, results );
	}
}


template<bool detail>
inline static void evaluate_rook_trapped( position const& p, color::type c, uint64_t rook, eval_results& results )
{
	// In middle-game, a rook trapped behind own pawns on the same side of a king that cannot castle is quite bad.
	// Getting that rook into play likely requires several tempi and/or might destroy structur of own position.
	uint64_t rook_file = rook % 8;

	uint64_t rook_rank = rook / 8;

	uint64_t king_file = p.king_pos[c] % 8;
	uint64_t king_rank = p.king_pos[c] / 8;

	if( king_rank == (c ? 7 : 0) && rook_rank == king_rank ) {
		// Kingside
		if( king_file >= 4 && rook_file > king_file ) {
			// Check for open file to the right of the king
			for( int f = static_cast<int>(king_file) + 1; f < 8; ++f ) {
				if( !((0x0101010101010101ull << f) & p.bitboards[c].b[bb_type::pawns]) ) {
					return;
				}
			}
			add_score<detail, eval_detail::trapped_rook>( results, c, eval_values::trapped_rook[(p.castle[c] & castles::both) ? 0 : 1] );
		}
		// Queenside
		else if( king_file < 4 && rook_file < king_file ) {
			// Check for open file to the left of the king
			for( int f = static_cast<int>(king_file) - 1; f >= 0; --f ) {
				if( !((0x0101010101010101ull << f) & p.bitboards[c].b[bb_type::pawns]) ) {
					return;
				}
			}
			// If we're queenside we can't castle anymore.
			add_score<detail, eval_detail::trapped_rook>( results, c, eval_values::trapped_rook[1] );
		}
	}
}


template<bool detail>
inline static void evaluate_rook_mobility( position const& p, color::type c, uint64_t rook, eval_results& results )
{
	uint64_t const all_blockers = (p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces]) & ~(p.bitboards[c].b[bb_type::rooks] | p.bitboards[c].b[bb_type::queens]);

	uint64_t moves = rook_magic( rook, all_blockers );

	if( moves & king_attack_zone[1-c][p.king_pos[1-c]] ) {
		++results.count_king_attackers[c];
		results.king_attacker_sum[c] += eval_values::king_attack_by_piece[pieces::rook];
	}

	results.attacks[c][pieces::rook] |= moves;

	if( moves & p.bitboards[c].b[bb_type::rooks] ) {
		// Is it bad that queen might be inbetween due to the X-RAY?
		add_score<detail, eval_detail::connected_rooks>( results, c, eval_values::connected_rooks );
	}


	moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawn_control]);
	uint64_t const move_count = popcount(moves);
	add_score<detail, eval_detail::mobility>( results, c, eval_values::mobility_rook[move_count] );

	if( move_count < 4 ) {
		evaluate_rook_trapped<detail>( p, c, rook, results );
	}
}


template<bool detail>
inline static void evaluate_rook_pin( position const& p, color::type c, uint64_t rook, eval_results& results )
{
	uint64_t own_blockers = p.bitboards[c].b[bb_type::all_pieces];
	uint64_t unblocked_moves = rook_magic( rook, own_blockers );

	if( unblocked_moves & p.bitboards[1-c].b[bb_type::king] ) {

		uint64_t between = between_squares[rook][p.king_pos[1-c]] & p.bitboards[1-c].b[bb_type::all_pieces];
		if( popcount( between ) == 1 ) {
			pieces::type piece = p.get_piece( bitscan( between ) );
			add_score<detail, eval_detail::absolute_pins>( results, c, eval_values::absolute_pin[ piece ] );
		}
	}
}


template<bool detail>
inline static void evaluate_rook_on_open_file( position const& p, color::type c, uint64_t rook, eval_results& results )
{
	uint64_t file = 0x0101010101010101ull << (rook % 8);
	if( !(p.bitboards[c].b[bb_type::pawns] & file) ) {
		if( p.bitboards[1-c].b[bb_type::pawns] & file ) {
			add_score<detail, eval_detail::rooks_on_open_file>( results, c, eval_values::rooks_on_half_open_file );
		}
		else {
			add_score<detail, eval_detail::rooks_on_open_file>( results, c, eval_values::rooks_on_open_file );
		}
	}
}


uint64_t const trapped_king[2]   = { 0x00000000000000ffull, 0xff00000000000000ull };
uint64_t const trapping_piece[2] = { 0x00ff000000000000ull, 0x000000000000ff00ull };

template<bool detail>
inline static void evaluate_rooks( position const& p, color::type c, eval_results& results )
{
	uint64_t rooks = p.bitboards[c].b[bb_type::rooks];

	while( rooks ) {
		uint64_t rook = bitscan_unset( rooks );

		add_score<detail, eval_detail::king_tropism>( results, c, eval_values::tropism[pieces::rook] * proximity[rook][p.king_pos[1-c]] );

		evaluate_rook_mobility<detail>( p, c, rook, results);
		evaluate_rook_pin<detail>( p, c, rook, results );
		evaluate_rook_on_open_file<detail>( p, c, rook, results );
	}

	// At least in endgame, enemy king trapped on its home rank is quite useful.
	if( (p.bitboards[c].b[bb_type::rooks] | p.bitboards[c].b[bb_type::queens]) & trapping_piece[c] && p.bitboards[1-c].b[bb_type::king] & trapped_king[1-c] ) {
		add_score<detail, eval_detail::rooks_on_7h_rank>( results, c, eval_values::rooks_on_rank_7 );
	}
}


template<bool detail>
inline static void evaluate_queen_mobility( position const& p, color::type c, uint64_t queen, eval_results& results )
{
	uint64_t const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];

	uint64_t moves = bishop_magic( queen, all_blockers ) | rook_magic( queen, all_blockers );

	if( moves & king_attack_zone[1-c][p.king_pos[1-c]] ) {
		++results.count_king_attackers[c];
		results.king_attacker_sum[c] += eval_values::king_attack_by_piece[pieces::queen];
	}

	results.attacks[c][pieces::queen] |= moves;

	moves &= ~(p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::pawn_control]);
	add_score<detail, eval_detail::mobility>( results, c, eval_values::mobility_queen[popcount(moves)] );
}


template<bool detail>
inline static void evaluate_queen_pin( position const& p, color::type c, uint64_t queen, eval_results& results )
{
	uint64_t own_blockers = p.bitboards[c].b[bb_type::all_pieces];
	uint64_t unblocked_moves = bishop_magic( queen, own_blockers ) | rook_magic( queen, own_blockers );

	if( unblocked_moves & p.bitboards[1-c].b[bb_type::king] ) {

		uint64_t between = between_squares[queen][p.king_pos[1-c]] & p.bitboards[1-c].b[bb_type::all_pieces];
		if( popcount( between ) == 1 ) {
			pieces::type piece = p.get_piece( bitscan( between ) );
			add_score<detail, eval_detail::absolute_pins>( results, c, eval_values::absolute_pin[ piece ] );
		}
	}
}


template<bool detail>
inline static void evaluate_queens( position const& p, color::type c, eval_results& results )
{
	uint64_t queens = p.bitboards[c].b[bb_type::queens];

	while( queens ) {
		uint64_t queen = bitscan_unset( queens );

		add_score<detail, eval_detail::king_tropism>( results, c, eval_values::tropism[pieces::queen] * proximity[queen][p.king_pos[1-c]] );

		evaluate_queen_mobility<detail>( p, c, queen, results );
		evaluate_queen_pin<detail>( p, c, queen, results );
	}
}


short advance_bonus[] = { 1, 1, 1, 2, 4, 8 };

template<bool detail>
void evaluate_passed_pawns( position const& p, color::type c, eval_results& results )
{
	for( int i = 0; i < 2; ++i ) {
		uint64_t passed = (p.bitboards[i].b[bb_type::pawns] & results.passed_pawns );
		uint64_t unstoppable = passed & ~rule_of_the_square[1-i][c][p.king_pos[1-i]];

		while( passed ) {
			uint64_t pawn = bitscan_unset( passed );

			uint64_t file = pawn % 8;

			uint64_t advance = i ? (6 - pawn / 8) : (pawn / 8 - 1);

			add_score<detail, eval_detail::passed_pawns>( results, static_cast<color::type>(i), (eval_values::passed_pawn_king_distance[0] * king_distance[pawn + (i ? -8 : 8)][p.king_pos[1-i]] - eval_values::passed_pawn_king_distance[1] * king_distance[pawn + (i ? -8 : 8)][p.king_pos[i]]) * advance_bonus[advance] );

			add_score<detail, eval_detail::passed_pawns>( results, static_cast<color::type>(i), eval_values::advanced_passed_pawn[file][advance] );

			uint64_t forward_squares = doubled_pawns[i][pawn];

			if( (1ull << pawn) & unstoppable ) {
				add_score<detail, eval_detail::passed_pawns>( results, static_cast<color::type>(i), eval_values::rule_of_the_square * advance_bonus[advance] );
			}
			uint64_t hinderers = (results.attacks[1-i][0] & ~results.attacks[i][0]) | p.bitboards[1-i].b[bb_type::all_pieces];
			if( !(forward_squares & hinderers) ) {
				add_score<detail, eval_detail::passed_pawns>( results, static_cast<color::type>(i), eval_values::passed_pawn_unhindered * advance_bonus[advance] );
			}
		}
	}
}

uint64_t base_king_attack[2][8] = {
	{
		0, 5, 12, 25, 25, 25, 25, 25
	},
	{
		25, 25, 25, 25, 25, 12, 5, 0
	}
};


template<bool detail>
static void evaluate_king_attack( position const& p, color::type c, eval_results& results )
{
	// Consider a lone attacker harmless
	if( results.count_king_attackers[c] < 2 ||
		!p.bitboards[c].b[bb_type::queens] ||
		p.material[c].mg() < (eval_values::material_values[pieces::queen] + eval_values::material_values[pieces::rook] ).mg() ) {
		return;
	}

	uint64_t attack = base_king_attack[1-c][p.king_pos[1-c] / 8];

	// Add all the attackers that can reach close to the king
	attack += results.king_attacker_sum[c];

	// Add all positions where a piece can safely give check
	uint64_t secure_squares = ~(results.attacks[1-c][0] | p.bitboards[c].b[bb_type::all_pieces]); // Don't remove enemy pieces, those we would just capture

	uint64_t knight_checks = possible_knight_moves[ p.king_pos[1-c] ] & secure_squares;
	uint64_t const all_blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];
	uint64_t bishop_checks = bishop_magic( p.king_pos[1-c], all_blockers ) & secure_squares;
	uint64_t rook_checks = rook_magic( p.king_pos[1-c], all_blockers ) & secure_squares;

	attack += popcount( results.attacks[c][pieces::knight] & knight_checks ) * eval_values::king_check_by_piece[pieces::knight];
	attack += popcount( results.attacks[c][pieces::bishop] & bishop_checks ) * eval_values::king_check_by_piece[pieces::bishop];
	attack += popcount( results.attacks[c][pieces::rook] & rook_checks ) * eval_values::king_check_by_piece[pieces::rook];
	attack += popcount( results.attacks[c][pieces::queen] & (bishop_checks|rook_checks) ) * eval_values::king_check_by_piece[pieces::queen];

	// Undefended squares around king are bad
	uint64_t const undefended = possible_king_moves[p.king_pos[1-c]] & ~(results.attacks[1-c][pieces::pawn] | results.attacks[1-c][pieces::knight] | results.attacks[1-c][pieces::bishop] | results.attacks[1-c][pieces::rook] | results.attacks[1-c][pieces::queen]);
	attack += 5 * popcount( undefended );

	// Rooks and queens that are able to move next to a king without getting captured are extremely dangerous.
	uint64_t const backed_queen_attacks = results.attacks[c][pieces::queen] & (results.attacks[c][pieces::pawn] | results.attacks[c][pieces::knight] | results.attacks[c][pieces::bishop] | results.attacks[c][pieces::rook]);
	uint64_t const backed_rook_attacks = results.attacks[c][pieces::rook] & (results.attacks[c][pieces::pawn] | results.attacks[c][pieces::knight] | results.attacks[c][pieces::bishop] | results.attacks[c][pieces::queen]);
	uint64_t const king_melee_attack_by_queen = undefended & backed_queen_attacks & ~p.bitboards[c].b[bb_type::all_pieces];
	uint64_t const king_melee_attack_by_rook = undefended & backed_rook_attacks & ~p.bitboards[c].b[bb_type::all_pieces] & rook_magic( p.king_pos[1-c], 0 );
	uint64_t const initiative = (p.self() == c) ? 2 : 1;
	attack += popcount( king_melee_attack_by_queen ) * eval_values::king_melee_attack_by_queen * initiative;
	attack += popcount( king_melee_attack_by_rook ) * eval_values::king_melee_attack_by_rook * initiative;

	// Take pawn shield into account
	int64_t signed_attack = static_cast<int64_t>(attack);
	signed_attack -= (results.pawn_shield[1-c] * 10) / eval_values::king_attack_pawn_shield;

	if( signed_attack < 0 ) {
		signed_attack = 0;
	}
	else if( signed_attack > 199 ) {
		signed_attack = 199;
	}

	// 5% bonus if attacker is the one to move
	if( p.self() == c ) {
		signed_attack += signed_attack / 95;
	}

	add_score<detail, eval_detail::king_attack>( results, c, eval_values::king_attack[signed_attack] );
}


void evaluate_pawn( uint64_t own_pawns, uint64_t foreign_pawns, color::type c, uint64_t pawn, eval_results& results, score* s )
{
	uint64_t file = pawn % 8;

	int opposed = (foreign_pawns & doubled_pawns[c][pawn]) ? 1 : 0;

	// Unfortunately this is getting too complex for me to make branchless
	bool doubled = (doubled_pawns[c][pawn] & own_pawns) != 0;
	if( doubled ) {
		s[c] += eval_values::doubled_pawn[opposed][file];
	}

	bool connected = (connected_pawns[c][pawn] & own_pawns) != 0;
	if( connected ) {
		s[c] += eval_values::connected_pawn[opposed][file];
	}

	bool isolated = !(isolated_pawns[pawn] & own_pawns);
	if( isolated ) {
		s[c] += eval_values::isolated_pawn[opposed][file];
	}

	bool passed = !(passed_pawns[c][pawn] & foreign_pawns);
	if( passed && !doubled ) {
		results.passed_pawns |= 1ull << pawn;
	}

	bool backwards = false;
	if( !passed && !connected && !isolated && !(forward_pawn_attack[1-c][pawn] & own_pawns ) ) {
		uint64_t opposition = forward_pawn_attack[c][pawn] & foreign_pawns;
		if( opposition ) {
			// Check for supporting pawns. A supporting pawn needs to be closer than enemy pawn.
			uint64_t support = forward_pawn_attack[c][pawn] & own_pawns;

			// Since we're not isolated, there's pawn on neighboring file.
			// No pawn is behind and we're not connecting. So pawn needs to be in front.
			ASSERT( support );

			if( c == color::white ) {
				if( static_cast<int>(bitscan( opposition )) - static_cast<int>(bitscan( support )) < 14 ) {
					backwards = true;
				}
			}
			else {
				if( static_cast<int>(bitscan_reverse( support )) - static_cast<int>(bitscan_reverse( opposition )) < 14 ) {
					backwards = true;
				}
			}
		}
	}

	if( backwards ) {
		s[c] += eval_values::backward_pawn[opposed][file];
	}

	bool candidate = false;
	if( !passed && !isolated && !backwards ) {
		// Candidate passer
		uint64_t file = 0x0101010101010101ull << (pawn % 8);
		uint64_t opposition = passed_pawns[c][pawn] & foreign_pawns;
		uint64_t support = forward_pawn_attack[1-c][pawn + (c ? -8 : 8)] & own_pawns;
		if( !(file & foreign_pawns) && popcount(opposition) <= popcount(support) ) {
			candidate = true;
		}
	}

	if( candidate ) {
		s[c] += eval_values::candidate_passed_pawn[file];
	}

#if 0
	std::cerr << "Pawn: " << std::setw(2) << pawn
			  << " Color: " << c
			  << " Doubled: " << doubled
			  << " Isolated: " << isolated
			  << " Connected: " << connected
			  << " Passed: " << passed
			  << " Candidate: " << candidate
			  << " Backwards: " << backwards
			  << std::endl;
#endif

}


template<bool detail>
void evaluate_pawns( position const& p, eval_results& results )
{
	score s[2];
	if( pawn_hash_table.lookup( p.pawn_hash, s, results.passed_pawns ) ) {
		add_score<detail, eval_detail::pawn_structure>( results, s );
		return;
	}

	for( int c = 0; c < 2; ++c ) {
		uint64_t own_pawns = p.bitboards[c].b[bb_type::pawns];
		uint64_t foreign_pawns = p.bitboards[1-c].b[bb_type::pawns];

		uint64_t pawns = own_pawns;
		while( pawns ) {
			uint64_t pawn = bitscan_unset( pawns );

			evaluate_pawn( own_pawns, foreign_pawns, static_cast<color::type>(c), pawn, results, s );
		}
	}

	add_score<detail, eval_detail::pawn_structure>( results, s );
	pawn_hash_table.store( p.pawn_hash, s, results.passed_pawns );
}


/* Pawn shield for king
 *
 * Count number of pawns ahead and beside of king, ranked by distance to home row.
 * Only consider pawns on relative ranks 2 to 5.
 * Idea is that pawns closer to king score more than pawns away from king. Also,
 * for a good score, king needs to be on home rank.
 */
template<bool detail, color::type c>
void evaluate_pawn_shield_side( position const& p, eval_results& results )
{
	int king_pos = p.king_pos[c];
	if( c ? king_pos < 24 : king_pos >= 40 ) {
		return;
	}

	// If king is at sides, shift it closer to middle.
	if( !(king_pos % 8) ) {
		++king_pos;
	}
	else if( (king_pos % 8) == 7 ) {
		--king_pos;
	}

	uint64_t pawns = (passed_pawns[c][king_pos] | 7ull << (king_pos - 1)) & p.bitboards[c].b[bb_type::pawns];
	uint64_t enemy_pawns = passed_pawns[c][king_pos] & p.bitboards[1-c].b[bb_type::pawns];
	uint64_t k = 0xffull << (c ? 48 : 8);
	score s;
	for( int i = 0; i < 4; ++i ) {
		s += eval_values::pawn_shield[i] * static_cast<short>(popcount(pawns & k) );
		s -= eval_values::pawn_shield_attack[i] * static_cast<short>(popcount(enemy_pawns & k) );
		k = c ? k >> 8 : k << 8;
	}
	add_score<detail, eval_detail::pawn_shield>( results, c, s );
	results.pawn_shield[c] = s.mg();
}


template<bool detail>
void evaluate_pawn_shield( position const& p, eval_results& results )
{
	evaluate_pawn_shield_side<detail, color::white>( p, results );
	evaluate_pawn_shield_side<detail, color::black>( p, results );
}


template<bool detail>
static void evaluate_drawishness( position const& p, color::type c, eval_results& results )
{
	if( p.bitboards[c].b[bb_type::pawns] ) {
		return;
	}

	short material_difference = p.material[c].eg() - p.material[1-c].eg();
	if( material_difference > 0 && material_difference <= eval_values::insufficient_material_threshold ) {
		add_score<detail, eval_detail::drawishness>( results, c, score( 0, eval_values::drawishness ) );
	}
}


template<bool detail>
static void evaluate_center( position const& p, color::type c, eval_results& results )
{
	// Not taken by own pawns nor under control by enemy pawns
	uint64_t potential_center_squares = central_squares[c] & ~(p.bitboards[c].b[bb_type::pawns] | p.bitboards[1-c].b[bb_type::pawn_control]);
	uint64_t safe_center_squares = potential_center_squares & results.attacks[c][0] & ~results.attacks[1-c][0];

	add_score<detail, eval_detail::center_control>( results, c, eval_values::center_control * static_cast<short>(popcount(safe_center_squares)) );
}

template<bool detail>
static void evaluate_piece_defense( position const& p, color::type c, eval_results& results )
{
	// Bonus for enemy pieces we attack that are not defended
	// Undefended are those attacked by self, not attacked by enemy
	uint64_t undefended = results.attacks[c][pieces::none] & ~results.attacks[1-c][pieces::none];
	uint64_t defended = results.attacks[c][pieces::none] & results.attacks[1-c][pieces::none];
	for( unsigned int piece = 1; piece < 6; ++piece ) {
		add_score<detail, eval_detail::hanging_pieces>( results, static_cast<color::type>(c), eval_values::hanging_piece[piece] * static_cast<short>(popcount( p.bitboards[1-c].b[piece] & undefended) ) );
		add_score<detail, eval_detail::attacked_pieces>( results, static_cast<color::type>(c), eval_values::attacked_piece[piece] * static_cast<short>(popcount( p.bitboards[1-c].b[piece] & defended) ) );
		add_score<detail, eval_detail::defended_by_pawn>( results, static_cast<color::type>(c), eval_values::defended_by_pawn[piece] * static_cast<short>(popcount( p.bitboards[c].b[piece] & p.bitboards[c].b[bb_type::pawn_control]) ) );
	}
}

template<bool detail>
static void evaluate_imbalance( position const& p, eval_results& results )
{
	// TODO: Check if it's faster to use p.piece_sum
	short pawns[2];
	pawns[0] = static_cast<short>( popcount( p.bitboards[0].b[bb_type::pawns] ) );
	pawns[1] = static_cast<short>( popcount( p.bitboards[1].b[bb_type::pawns] ) );
	short pawn_diff = pawns[0] - pawns[1];

	short minors[2];
	minors[0] = static_cast<short>( popcount( p.bitboards[0].b[bb_type::knights] | p.bitboards[0].b[bb_type::bishops] ) );
	minors[1] = static_cast<short>( popcount( p.bitboards[1].b[bb_type::knights] | p.bitboards[1].b[bb_type::bishops] ) );
	short minor_diff = minors[0] - minors[1];

	// TODO: Somehow handle queens
	short majors[2];
	majors[0] = static_cast<short>( popcount( p.bitboards[0].b[bb_type::rooks] ) );
	majors[1] = static_cast<short>( popcount( p.bitboards[1].b[bb_type::rooks] )) ;
	short major_diff = majors[0] - majors[1];

	score v;
	while( minor_diff > 1 && major_diff < 0 ) {
		v += eval_values::material_imbalance[1];
		minor_diff -= 2;
		++major_diff;
	}
	while( minor_diff < -1 && major_diff > 0 ) {
		v -= eval_values::material_imbalance[1];
		minor_diff += 2;
		--major_diff;
	}
	while( pawn_diff > 2 && minor_diff < 0 ) {
		pawn_diff -= 3;
		++minor_diff;
	}
	while( pawn_diff < -2 && minor_diff > 0 ) {
		pawn_diff += 3;
		--minor_diff;
	}
	v += eval_values::material_imbalance[0] * minor_diff;

	add_score<detail, eval_detail::imbalance>( results, color::white, v );
}

template<bool detail>
static void do_evaluate( position const& p, eval_results& results )
{
	evaluate_pawns<detail>( p, results );

	for( unsigned int c = 0; c < 2; ++c ) {
		if( popcount( p.bitboards[c].b[bb_type::bishops] ) > 1 ) {
			add_score<detail, eval_detail::double_bishop>( results, static_cast<color::type>(c), eval_values::double_bishop );
		}
	}

	evaluate_imbalance<detail>( p, results );

	evaluate_pawn_shield<detail>( p, results );

	add_score<detail, eval_detail::side_to_move>( results, p.self(), eval_values::side_to_move );

	for( unsigned int c = 0; c < 2; ++c ) {
		results.attacks[c][pieces::pawn] = p.bitboards[c].b[bb_type::pawn_control];
		evaluate_pawns_mobility<detail>( p, static_cast<color::type>(c), results );
		evaluate_knights<detail>( p, static_cast<color::type>(c), results );
		evaluate_bishops<detail>( p, static_cast<color::type>(c), results );
		evaluate_rooks<detail>( p, static_cast<color::type>(c), results );
		evaluate_queens<detail>( p, static_cast<color::type>(c), results );

		//Piece-square tables already contain this
		//results.center_control += static_cast<short>(popcount(p.bitboards[c].b[bb_type::all_pieces] & center_squares));

		results.attacks[c][pieces::none] =
				results.attacks[c][pieces::pawn] |
				results.attacks[c][pieces::knight] |
				results.attacks[c][pieces::bishop] |
				results.attacks[c][pieces::rook] |
				results.attacks[c][pieces::queen] |
				possible_king_moves[p.king_pos[c]];
	}

	for( unsigned int c = 0; c < 2; ++c ) {

		evaluate_piece_defense<detail>( p, static_cast<color::type>(c), results );
		evaluate_king_attack<detail>( p, static_cast<color::type>(c), results );
		//evaluate_drawishness<detail>( p, static_cast<color::type>(c), results );
		evaluate_center<detail>( p, static_cast<color::type>(c), results );
	}

	evaluate_passed_pawns<detail>( p, p.self(), results );
}

score sum_up( position const& p, eval_results const& results ) {
	return p.base_eval + results.eval[color::white] - results.eval[color::black];
}
}

namespace {
static short scale( position const& p, score const& s )
{
	short mat = p.material[0].mg() + p.material[1].mg();
	short ev = s.scale( mat );

	// Check for endgames with opposite colored bishops, those are rather drawish :(
	if( p.material[0].mg() == eval_values::material_values[pieces::bishop].mg() &&
		p.material[1].mg() == eval_values::material_values[pieces::bishop].mg() )
	{
		uint64_t wb = bitscan( p.bitboards[0].b[bb_type::bishops] );
		uint64_t bb = bitscan( p.bitboards[1].b[bb_type::bishops] );
		if( (wb % 2) != (bb % 2) ) {
			ev /= 2;
		}
	}
	return ev;
}


static std::string explain( position const& p, const char* name, score const& data ) {
	std::stringstream ss;
	ss << std::setw(19) << name << " |             |             | " << std::setw(5) << data.mg() << " " << std::setw(5) << data.eg() << " " << std::setw(5) << scale( p, data ) << std::endl;
	return ss.str();
}


static std::string explain( position const& p, const char* name, score const* data ) {
	std::stringstream ss;
	ss << std::setw(19) << name << " | ";
	ss << std::setw(5) << data[0].mg() << " " << std::setw(5) << data[0].eg() << " | ";
	ss << std::setw(5) << data[1].mg() << " " << std::setw(5) << data[1].eg() << " | ";
	score total = data[0] - data[1];
	ss << std::setw(5) << total.mg() << " " << std::setw(5) << total.eg() << " " << std::setw(5) << scale( p, data[0]-data[1] ) << std::endl;
	return ss.str();
}


static std::string explain( position const& p, const char* name, eval_detail::type t ) {
	std::stringstream ss;

	score const* s = detailed_results[t];
	if( s[0] != score() || s[1] != score() ) {
		ss << std::setw(19) << name << " | ";
		ss << std::setw(5) << s[0].mg() << " " << std::setw(5) << s[0].eg() << " | ";
		ss << std::setw(5) << s[1].mg() << " " << std::setw(5) << s[1].eg() << " | ";
		score total = s[0] - s[1];
		ss << std::setw(5) << total.mg() << " " << std::setw(5) << total.eg() << " " << std::setw(5) << scale( p, total ) << std::endl;
	}
	return ss.str();
}


}


std::string explain_eval( position const& p )
{
	std::stringstream ss;

	short endgame = 0;
	if( evaluate_endgame( p, endgame ) ) {
		ss << "Endgame: " << endgame << std::endl;
	}
	else {
		for( int i = 0; i < eval_detail::max; ++i ) {
			detailed_results[i][0] = score();
			detailed_results[i][1] = score();
		}

		eval_results results;
		do_evaluate<true>( p, results );

		score full = sum_up( p, results );

		ss << "                    |    White    |    Black    |       Total" << std::endl;
		ss << "         Term       |   MG   EG   |   MG   EG   |   MG   EG  Scaled" << std::endl;
		ss << "===================================================================" << std::endl;
		ss << explain( p, "Material", p.material );
		if( detailed_results[eval_detail::imbalance][0] != score() ) {
			ss << explain( p, "Imbalance", detailed_results[eval_detail::imbalance][0] );
		}
		score pst = p.base_eval-p.material[0]+p.material[1] -
			eval_values::material_values[pieces::pawn] * static_cast<short>(popcount(p.bitboards[0].b[bb_type::pawns])) +
			eval_values::material_values[pieces::pawn] * static_cast<short>(popcount(p.bitboards[1].b[bb_type::pawns]));

		ss << explain( p, "PST", pst );
		ss << explain( p, "Pawn structure", eval_detail::pawn_structure );
		ss << explain( p, "Pawn shield", eval_detail::pawn_shield );
		ss << explain( p, "Passed pawns", eval_detail::passed_pawns );
		ss << explain( p, "King attack", eval_detail::king_attack );
		ss << explain( p, "King tropism", eval_detail::king_tropism );
		ss << explain( p, "Mobility",  eval_detail::mobility );
		ss << explain( p, "Center control", eval_detail::center_control );
		ss << explain( p, "Absolute pins", eval_detail::absolute_pins );
		ss << explain( p, "Defended by pawn", eval_detail::defended_by_pawn );
		ss << explain( p, "Attacked pieces", eval_detail::attacked_pieces );
		ss << explain( p, "Hanging pieces", eval_detail::hanging_pieces );
		ss << explain( p, "Connected rooks", eval_detail::connected_rooks );
		ss << explain( p, "Rooks on open file", eval_detail::rooks_on_open_file );
		ss << explain( p, "Rooks on 7th rank", eval_detail::rooks_on_7h_rank );
		ss << explain( p, "Trapped rook", eval_detail::trapped_rook );
		ss << explain( p, "Outposts", eval_detail::outposts );
		ss << explain( p, "Double bishop", eval_detail::double_bishop );
		ss << explain( p, "Side to move", eval_detail::side_to_move );
		ss << explain( p, "Drawishness", eval_detail::drawishness );

		ss << "===================================================================" << std::endl;
		ss << explain( p, "Total", full );
	}

	return ss.str();
}


short evaluate_full( position const& p )
{
	short eval = 0;
	if( evaluate_endgame( p, eval ) ) {
		if( !p.white() ) {
			eval = -eval;
		}
		return eval;
	}

	eval_results results;
	do_evaluate<false>( p, results );

	score full = sum_up( p, results );
	
	eval = scale( p, full );
	if( !p.white() ) {
		eval = -eval;
	}

	ASSERT( eval > result::loss && eval < result::win );

	return eval;
}
