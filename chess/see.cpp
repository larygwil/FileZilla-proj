#include "see.hpp"
#include "sliding_piece_attacks.hpp"
#include "eval.hpp"
#include "platform.hpp"

#include <algorithm>
#include <iostream>

extern unsigned long long const possible_knight_moves[64];
extern unsigned long long const possible_king_moves[64];
extern unsigned long long pawn_control[2][64];

static unsigned long long least_valuable_attacker( position const& p, color::type c, unsigned long long attackers, pieces::type& attacker_piece_out ) {
	// Exploit that our bitboards are sorted by piece value
	for( int piece = pieces::pawn; piece <= pieces::king; ++piece ) {
		unsigned long long match = p.bitboards[c].b[piece] & attackers;
		if( match ) {
			attacker_piece_out = static_cast<pieces::type>(piece);
			unsigned long long ret;
			bitscan( match, ret );
			return 1ull << ret;
		}
	}

	return 0;
}

int see( position const& p, color::type c, move const& m )
{
	// Iterative SEE algorithm as described by Fritz Reul, adapted to use bitboards.
	unsigned char target = m.target;

	unsigned long long attacker_mask = 1ull << m.source;
	pieces::type attacker_piece = m.piece;

	int score[32];

	unsigned long long const all_rooks = p.bitboards[color::white].b[bb_type::rooks] | p.bitboards[color::white].b[bb_type::queens] | p.bitboards[color::black].b[bb_type::rooks] | p.bitboards[color::black].b[bb_type::queens];
	unsigned long long const all_bishops = p.bitboards[color::white].b[bb_type::bishops] | p.bitboards[color::white].b[bb_type::queens] | p.bitboards[color::black].b[bb_type::bishops] | p.bitboards[color::black].b[bb_type::queens];

	unsigned long long all_pieces = p.bitboards[color::white].b[bb_type::all_pieces] | p.bitboards[color::black].b[bb_type::all_pieces];

	unsigned long long attackers =
			(rook_attacks( target, all_pieces ) & all_rooks) |
			(bishop_attacks( target, all_pieces ) & all_bishops) |
			(possible_knight_moves[target] & (p.bitboards[color::white].b[bb_type::knights] | p.bitboards[color::black].b[bb_type::knights])) |
			(possible_king_moves[target] & (p.bitboards[color::white].b[bb_type::king] | p.bitboards[color::black].b[bb_type::king])) |
			(pawn_control[color::black][target] & p.bitboards[color::white].b[bb_type::pawns]) |
			(pawn_control[color::white][target] & p.bitboards[color::black].b[bb_type::pawns]);

	int depth = 0;

	score[0] = get_material_value( m.captured_piece );

	// Can "do", as we always have at least one.
	do {
		++depth;
		score[depth] = get_material_value( attacker_piece ) - score[depth - 1];

		if( score[depth] < 0 && score[depth - 1] < 0 ) {
			break; // Bad capture
		}

		// Remove old attacker
		all_pieces ^= attacker_mask;
		attackers ^= attacker_mask;

		// Update attacker list due to uncovered attacks
		// Todo: Only update specific ray
		attackers |= ((all_pieces & rook_attacks( target, all_pieces )) & all_rooks) |
						((all_pieces & bishop_attacks( target, all_pieces )) & all_bishops);

		if( !attackers ) {
			break;
		}

		// Get new attacker
		attacker_mask = least_valuable_attacker( p, static_cast<color::type>(c ^ (depth & 1)), attackers, attacker_piece );
	} while( true );

	// Propagate scores back
	while( --depth ) {
		score[depth - 1] = -(std::max)(score[depth], -score[depth - 1]);
	}

	return score[0];
}
