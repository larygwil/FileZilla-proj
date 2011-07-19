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
 *   10cccrrr otherwise with ccc and rrr being column and row of the check giving piece.
 * col/row of other own pieces:
 *   00000000 if piece is not blocking check
 *   10cccrrr of the piece giving check if own piece were not there
 * col/row of enemy pieces:
 *   00000000 if not giving check
 *   10cccrrr if giving check
 * col/row of empty fields:
 *   00000000 if not in line of sight between own king and enemy piece and not blocked by at least two own pieces
 *   10cccrrr otherwise
 */
struct check_map
{
	unsigned char board[8][8];
	unsigned char check;

	inline bool multiple() const { return check & 0x40; }
};
void calc_check_map( position const& p, color::type c, check_map& map );

bool detect_check( position const& p, color::type c, unsigned char king_col, unsigned char king_row, unsigned char ignore_col, unsigned char ignore_row );
bool detect_check( position const& p, color::type c );

#endif
