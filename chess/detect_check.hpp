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
class check_map
{
public:
	check_map( position const& p );

	unsigned char board[64];
	unsigned char check;

	inline bool multiple() const { return (check & 0x40) != 0; }
};

bool detect_check( position const& p, color::type c, uint64_t king, uint64_t ignore );
bool detect_check( position const& p, color::type c );

#endif
