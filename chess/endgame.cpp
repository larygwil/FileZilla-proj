#include "endgame.hpp"

#include <iostream>

enum piece_masks {
	white_pawn =   0x0,
	white_knight = 0x10,
	white_bishop = 0x100,
	white_rook =   0x1000,
	white_queen =  0x10000,
	black_pawn =   0x100000,
	black_knight = 0x1000000,
	black_bishop = 0x10000000,
	black_rook =   0x100000000,
	black_queen =  0x1000000000,
	max         =  0x10000000000
};


bool evaluate_endgame( position const& p, short& result )
{
	uint64_t piece_sum = 0;

	for( int c = 0; c < 2; ++c ) {
		for( int piece = pieces::pawn; piece < pieces::king; ++piece ) {
			uint64_t count = popcount( p.bitboards[c].b[piece] );
			piece_sum |= count << ((piece - 1 + (c ? 5 : 0)) * 4);
		}
	}

	switch( piece_sum ) {
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
	default:
		break;
	}

	return false;
}
