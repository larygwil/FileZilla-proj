#include "endgame.hpp"
#include "tables.hpp"

#include <iostream>

enum piece_masks : uint64_t {
	white_pawn =   0x1ull,
	white_knight = 0x10ull,
	white_bishop = 0x100ull,
	white_rook =   0x1000ull,
	white_queen =  0x10000ull,
	black_pawn =   0x100000ull,
	black_knight = 0x1000000ull,
	black_bishop = 0x10000000ull,
	black_rook =   0x100000000ull,
	black_queen =  0x1000000000ull,
	max         =  0x10000000000ull
};

uint64_t const light_squared_bishop_mask = 0x55aa55aa55aa55aaull;

short evaluate_KNBvK( position const& p, color::type c )
{
	// This is a very favorable position
	short eval = 20000;

	// This drives enemy king first to the edge of the board, then into the right corner
	int corner_distance;
	if( p.bitboards[c].b[bb_type::bishops] & light_squared_bishop_mask ) {
		corner_distance = std::min( king_distance[p.king_pos[1-c]][7], king_distance[p.king_pos[1-c]][56] );
	}
	else {
		corner_distance = std::min( king_distance[p.king_pos[1-c]][0], king_distance[p.king_pos[1-c]][63] );
	}

	int x = p.king_pos[1-c] % 8;
	int y = p.king_pos[1-c] / 8;
	if( x > 3 ) {
		x = 7 - x;
	}
	if( y > 3 ) {
		y = 7 - x;
	}
	int edge_distance = std::min( x, y );

	eval += 45 * (7 - corner_distance );
	eval += 55 * (3 - edge_distance );

	// Reward closeness of own king and knight to enemy king
	eval += 5 * (7 - king_distance[p.king_pos[c]][p.king_pos[1-c]]);
	eval += 7 - king_distance[bitscan(p.bitboards[c].b[bb_type::knights])][p.king_pos[1-c]];

	return eval;
}


bool evaluate_KPvK( position const& p, color::type c, short& result )
{
	// Drawn if pawn on a or h file, enemy king in front
	uint64_t pawn = bitscan(p.bitboards[c].b[bb_type::pawns]);
	if( p.bitboards[c].b[bb_type::pawns] & 0x8181818181818181ull ) {
		if( passed_pawns[c][pawn] & p.bitboards[1-c].b[bb_type::king] ) {
			result = result::draw;
			return true;
		}
	}

	// Won if enemy king cannot catch pawn
	uint64_t unstoppable = p.bitboards[c].b[bb_type::pawns] & ~rule_of_the_square[1-c][p.c][p.king_pos[1-c]];
	if( unstoppable ) {
		if( c == color::black ) {
			result = result::win_threshold + static_cast<short>(pawn / 8);
		}
		else {
			result = result::loss_threshold - 7 + static_cast<short>(pawn / 8);
		}
		return true;
	}
	return false;
}

bool evaluate_KBPvKP( position const& p, color::type c, short& result )
{
	// This endgame is drawn if pawn on home rank on a, b, g or h with kind behind it or in adjacent corner and enemy pawn ahead of it.
	// As in: 8/8/8/b7/8/3k2p1/6P1/7K w - -
	// Color of bishop does not matter.
	
	// Special exception: 8/8/8/3b4/8/5k1p/7P/7K b - - 0 1
	// We rely on search finding it.

	if( !((p.bitboards[c].b[bb_type::pawns]) & (c ? 0x0000000000C30000ull : 0x0000C30000000000ull)) ) {
		return false;
	}

	uint64_t own_pawn = bitscan( p.bitboards[c].b[bb_type::pawns] );

	uint64_t other_pawn = own_pawn;
	if( c ) {
		other_pawn -= 8;
	}
	else {
		other_pawn += 8;
	}
	if( p.bitboards[1-c].b[bb_type::pawns] != (1ull << other_pawn) ) {
		return false;
	}

	int king_pos = p.king_pos[1-c];
	if( king_pos / 8 != (c ? 0 : 7 ) ) {
		return false;
	}

	if( own_pawn % 8 < 2 ) {
		if( king_pos % 8 > 1 ) {
			return false;
		}
	}
	else {
		if( king_pos % 8 < 6 ) {
			return false;
		}
	}

	result = result::draw;

	return true;
}

bool evaluate_KPvKP( position const& p, color::type c, short& result )
{
	// This endgame is drawn if pawn on home rank on a, b, g or h with kind behind it or in adjacent corner and enemy pawn ahead of it.
	// As in: 8/8/8/8/8/3k2p1/6P1/7K w - -
	// Color of bishop does not matter.

	if( !((p.bitboards[c].b[bb_type::pawns]) & (c ? 0x0000000000C30000ull : 0x0000C30000000000ull)) ) {
		return false;
	}

	uint64_t own_pawn = bitscan( p.bitboards[c].b[bb_type::pawns] );

	uint64_t other_pawn = own_pawn;
	if( c ) {
		other_pawn -= 8;
	}
	else {
		other_pawn += 8;
	}
	if( p.bitboards[1-c].b[bb_type::pawns] != (1ull << other_pawn) ) {
		return false;
	}

	int king_pos = p.king_pos[1-c];
	if( king_pos / 8 != (c ? 0 : 7 ) ) {
		return false;
	}

	if( own_pawn % 8 < 2 ) {
		if( king_pos % 8 > 1 ) {
			return false;
		}
	}
	else {
		if( king_pos % 8 < 6 ) {
			return false;
		}
	}

	result = 0;

	return true;
}

bool evaluate_endgame( position const& p, short& result )
{
	switch( p.piece_sum ) {
	// Totally insufficient material.
	case 0:
	case white_knight:
	case black_knight:
	case white_bishop:
	case black_bishop:
		result = result::draw;
		return true;

	// Usually drawn pawnless endgames, equal combinations. Mate only possible if enemy is one epic moron.
	case white_bishop + black_knight:
	case white_knight + black_bishop:
	case white_bishop + black_bishop:
	case white_knight + black_knight:
		result = result::draw;
		return true;

	// Usually drawn pawnless endgames, equal combinations. Still possible to force a mate if enemy loses a piece.
	case white_queen + black_queen:
	case white_rook + black_rook:
		result = (p.base_eval.eg() - p.material[0].eg() + p.material[1].eg()) / 5;
		return true;

	// Usually drawn pawnless endgames, imbalanced combinations.
	case white_rook + black_bishop:
	case white_bishop + black_rook:
	case white_rook + black_knight:
	case white_knight + black_rook:
	case white_rook + white_bishop + black_rook:
	case white_rook + black_rook + black_bishop:
	case white_rook + white_knight + black_rook:
	case white_rook + black_rook + black_knight:
	case white_queen + 2 * black_knight:
	case 2 * white_knight + black_queen:
	case white_queen + white_bishop + black_queen:
	case white_queen + black_bishop + black_queen:
		result = (p.base_eval.eg() - p.material[0].eg() + p.material[1].eg()) / 5;
		return true;
	// Drawn but dangerous for the defender.
	case white_queen + 2 * black_bishop:
	case white_queen + white_knight + black_queen:
		result = p.base_eval.eg() - p.material[0].eg() + p.material[1].eg() + 100;
		return true;
	case black_queen + 2 * white_bishop:
	case white_queen + black_knight + black_queen:
		result = p.base_eval.eg() - p.material[0].eg() + p.material[1].eg() - 100;
		return true;

	// Usually drawn endgames pawn vs. minor piece
	// If not drawn, search will hopefully save us.
	case white_bishop + black_pawn:
	case black_bishop + white_pawn:
	case white_knight + black_pawn:
	case black_knight + white_pawn:
		result = (p.base_eval.eg() - p.material[0].eg() + p.material[1].eg()) / 5;
		return true;

	case white_pawn:
		return evaluate_KPvK(p, color::white, result);
	case black_pawn:
		return evaluate_KPvK(p, color::black, result);
	// Drawn if bishop doesn't control the promotion square and enemy king is on promotion square or next to it
	case white_bishop + white_pawn:
		if( p.bitboards[color::white].b[bb_type::pawns] & 0x8181818181818181ull ) {
			uint64_t pawn = bitscan(p.bitboards[color::white].b[bb_type::pawns]);
			uint64_t enemy_king_mask = (pawn % 8) ? 0xc0c0000000000000ull : 0x0303000000000000ull;
			if( enemy_king_mask & p.bitboards[color::black].b[bb_type::king] ) {
				bool promotion_square_is_light = (pawn % 8) == 0;
				bool is_light_squared_bishop = (p.bitboards[color::white].b[bb_type::bishops] & light_squared_bishop_mask) != 0;
				if( promotion_square_is_light != is_light_squared_bishop ) {
					result = (p.base_eval.eg() - p.material[0].eg() + p.material[1].eg()) / 5;
					return true;
				}
			}
		}
		break;
	case black_bishop + black_pawn:
		if( p.bitboards[color::black].b[bb_type::pawns] & 0x8181818181818181ull ) {
			uint64_t pawn = bitscan(p.bitboards[color::black].b[bb_type::pawns]);
			uint64_t enemy_king_mask = (pawn % 8) ? 0xc0c0ull : 0x0303ull;
			if( enemy_king_mask & p.bitboards[color::white].b[bb_type::king] ) {
				bool promotion_square_is_light = (pawn % 8) == 7;
				bool is_light_squared_bishop = (p.bitboards[color::black].b[bb_type::bishops] & light_squared_bishop_mask) != 0;
				if( promotion_square_is_light != is_light_squared_bishop ) {
					result = (p.base_eval.eg() - p.material[0].eg() + p.material[1].eg()) / 5;
					return true;
				}
			}
		}
		break;

	// Easily drawn if opposite colored bishops
	case white_bishop + black_bishop + white_pawn:
	case white_bishop + black_bishop + black_pawn:
		{
			bool is_light_squared_white_bishop = ((p.bitboards[color::white].b[bb_type::bishops] & light_squared_bishop_mask) != 0);
			bool is_light_squared_black_bishop = ((p.bitboards[color::black].b[bb_type::bishops] & light_squared_bishop_mask) != 0);
			if( is_light_squared_white_bishop != is_light_squared_black_bishop ) {
				result = (p.base_eval.eg() - p.material[0].eg() + p.material[1].eg()) / 5;
				return true;
			}
		}
		break;
	case white_bishop + white_knight:
		result = evaluate_KNBvK( p, color::white );
		return true;
	case black_bishop + black_knight:
		result = -evaluate_KNBvK( p, color::black );
		return true;
	case white_bishop + white_pawn + black_pawn:
		return evaluate_KBPvKP( p, color::white, result );
	case black_bishop + black_pawn + white_pawn:
		return evaluate_KBPvKP( p, color::black, result );
	case white_pawn + black_pawn:
		if( !evaluate_KPvKP( p, p.self(), result ) ) {
			return evaluate_KPvKP( p, p.other(), result );
		}
	default:
		break;
	}

	return false;
}
