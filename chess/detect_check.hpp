#ifndef __DETECT_CHECK_H__
#define __DETECT_CHECK_H__

#include "chess.hpp"

/*
 * Check map structure, indicates which pieces give check an which pieces block check.
 * Used mainly in move calculation.
 *
 * col/row of own king:
 *   00000000 if not checked,
 *   11000000 if king is checked by multiple pieces,
 *   10rrrccc otherwise with ccc and rrr being column and row of the check giving piece.
 * col/row of other own pieces:
 *   00000000 if piece is not blocking check
 *   10rrrccc of the piece giving check if own piece were not there
 * col/row of enemy pieces:
 *   00000000 if not giving check
 *   10rrrccc if giving check
 * col/row of empty fields:
 *   00000000 if not in line of sight between own king and enemy piece and not blocked by at least two own pieces
 *   10rrrccc otherwise
 */
struct check_map
{
	unsigned char board[64];
	unsigned char check;

	inline bool multiple() const { return (check & 0x40) != 0; }
};
void calc_check_map( position const& p, color::type c, check_map& map );

bool detect_check( position const& p, color::type c, unsigned char king, unsigned char ignore );
bool detect_check( position const& p, color::type c );

/*
 * col/row of enemy king:
 *   00000000 always
 * empty space, other enemy pieces:
 *   10xxxxxx if on king horizontal or vertical with direct sight
 *   11xxxxxx if on king diagonals with direct sight
 *   0xxxxxxx otherwise
 * own piece:
 *   10rrrccc if blocking checks with ccc and rrr being column and row of the blocked piece
 *   00000000 otherwise
 */

struct inverse_check_map
{
	unsigned char board[64];
	unsigned char enemy_king_col;
	unsigned char enemy_king_row;
};

void calc_inverse_check_map( position const& p, color::type c, inverse_check_map& map );

#endif
