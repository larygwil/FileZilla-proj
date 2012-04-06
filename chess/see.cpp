#include "see.hpp"
#include "sliding_piece_attacks.hpp"
#include "eval.hpp"
#include "eval_values.hpp"
#include "magic.hpp"
#include "platform.hpp"
#include "tables.hpp"

#include <algorithm>
#include <iostream>

static uint64_t least_valuable_attacker( position const& p, color::type c, uint64_t attackers, pieces::type& attacker_piece_out )
{
	// Exploit that our bitboards are sorted by piece value and that we do have an attacker.
	uint64_t match;

	int piece = pieces::pawn;
	while( !(match = p.bitboards[c].b[piece] & attackers) ) {
		++piece;
	}

	attacker_piece_out = static_cast<pieces::type>(piece);
	uint64_t ret = bitscan( match );
	return 1ull << ret;
}


int see( position const& p, color::type c, move const& m )
{
	// Iterative SEE algorithm as described by Fritz Reul, adapted to use bitboards.
	unsigned char target = m.target;

	pieces::type attacker_piece = m.piece;

	int score[32];

	uint64_t const all_rooks = p.bitboards[color::white].b[bb_type::rooks] | p.bitboards[color::white].b[bb_type::queens] | p.bitboards[color::black].b[bb_type::rooks] | p.bitboards[color::black].b[bb_type::queens];
	uint64_t const all_bishops = p.bitboards[color::white].b[bb_type::bishops] | p.bitboards[color::white].b[bb_type::queens] | p.bitboards[color::black].b[bb_type::bishops] | p.bitboards[color::black].b[bb_type::queens];

	uint64_t all_pieces = p.bitboards[color::white].b[bb_type::all_pieces] | p.bitboards[color::black].b[bb_type::all_pieces];
	all_pieces ^= 1ull << m.source;

	uint64_t attackers =
			(rook_magic( target, all_pieces ) & all_rooks) |
			(bishop_magic( target, all_pieces ) & all_bishops) |
			(possible_knight_moves[target] & (p.bitboards[color::white].b[bb_type::knights] | p.bitboards[color::black].b[bb_type::knights])) |
			(possible_king_moves[target] & (p.bitboards[color::white].b[bb_type::king] | p.bitboards[color::black].b[bb_type::king])) |
			(pawn_control[color::black][target] & p.bitboards[color::white].b[bb_type::pawns]) |
			(pawn_control[color::white][target] & p.bitboards[color::black].b[bb_type::pawns]);
	// Don't have to remove source piece here, done implicitly in the loop.

	score[0] = eval_values::mg_material_values[ m.captured_piece ];

	// Get new attacker
	if( !(attackers & p.bitboards[1-c].b[bb_type::all_pieces]) ) {
		return score[0];
	}

	int depth = 1;

	// Can "do", as we always have at least one.
	do {
		score[depth] = eval_values::mg_material_values[ attacker_piece ] - score[depth - 1];

		if( score[depth] < 0 && score[depth - 1] < 0 ) {
			break; // Bad capture
		}

		// We have verified that there's already at least one attacker, so this works.
		uint64_t attacker_mask = least_valuable_attacker( p, static_cast<color::type>(c ^ (depth & 1)), attackers, attacker_piece );

		++depth;

		// Remove old attacker
		all_pieces ^= attacker_mask;

		// Update attacker list due to uncovered attacks
		attackers |= ((rook_magic( target, all_pieces )) & all_rooks) |
						((bishop_magic( target, all_pieces )) & all_bishops);
		attackers &= all_pieces;

		if( !(attackers & p.bitboards[c ^ (depth & 1)].b[bb_type::all_pieces]) ) {
			break;
		}
	} while( true );

	// Propagate scores back
	while( --depth ) {
		score[depth - 1] = -(std::max)(score[depth], -score[depth - 1]);
	}

	return score[0];
}
