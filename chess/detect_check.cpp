#include "chess.hpp"
#include "detect_check.hpp"

extern unsigned long long const possible_knight_moves[64] = {
	0x0000000000020400ull,
	0x0000000000050800ull,
	0x00000000000a1100ull,
	0x0000000000142200ull,
	0x0000000000284400ull,
	0x0000000000508800ull,
	0x0000000000a01000ull,
	0x0000000000402000ull,
	0x0000000002040004ull,
	0x0000000005080008ull,
	0x000000000a110011ull,
	0x0000000014220022ull,
	0x0000000028440044ull,
	0x0000000050880088ull,
	0x00000000a0100010ull,
	0x0000000040200020ull,
	0x0000000204000402ull,
	0x0000000508000805ull,
	0x0000000a1100110aull,
	0x0000001422002214ull,
	0x0000002844004428ull,
	0x0000005088008850ull,
	0x000000a0100010a0ull,
	0x0000004020002040ull,
	0x0000020400040200ull,
	0x0000050800080500ull,
	0x00000a1100110a00ull,
	0x0000142200221400ull,
	0x0000284400442800ull,
	0x0000508800885000ull,
	0x0000a0100010a000ull,
	0x0000402000204000ull,
	0x0002040004020000ull,
	0x0005080008050000ull,
	0x000a1100110a0000ull,
	0x0014220022140000ull,
	0x0028440044280000ull,
	0x0050880088500000ull,
	0x00a0100010a00000ull,
	0x0040200020400000ull,
	0x0204000402000000ull,
	0x0508000805000000ull,
	0x0a1100110a000000ull,
	0x1422002214000000ull,
	0x2844004428000000ull,
	0x5088008850000000ull,
	0xa0100010a0000000ull,
	0x4020002040000000ull,
	0x0400040200000000ull,
	0x0800080500000000ull,
	0x1100110a00000000ull,
	0x2200221400000000ull,
	0x4400442800000000ull,
	0x8800885000000000ull,
	0x100010a000000000ull,
	0x2000204000000000ull,
	0x0004020000000000ull,
	0x0008050000000000ull,
	0x00110a0000000000ull,
	0x0022140000000000ull,
	0x0044280000000000ull,
	0x0088500000000000ull,
	0x0010a00000000000ull,
	0x0020400000000000ull
};

bool detect_check_knight( position const& p, color::type c, int king_col, int king_row, piece const& pp, unsigned char pi )
{
	signed char cx = king_col - pp.column;
	if( !cx ) {
		return false;
	}

	signed char cy = king_row - pp.row;
	if( !cy ) {
		return false;
	}

	signed sum;
	if( cx > 0 ) {
		sum = cx;
	}
	else {
		sum = -cx;
	}
	if( cy > 0 ) {
		sum += cy;
	}
	else {
		sum -= cy;
	}
	if( sum != 3 ) {
		return false;
	}

	return true;
}

bool detect_check_knights( position const& p, color::type c, int king_col, int king_row )
{
	unsigned long long knights = possible_knight_moves[ king_col + king_row * 8 ];

	bool ret = false;
	{
		piece const& pp = p.pieces[1-c][pieces::knight1];
		if( pp.alive ) {
			ret |= knights & (1ull << (pp.column + pp.row * 8) );
		}
	}
	{
		piece const& pp = p.pieces[1-c][pieces::knight2];
		if( pp.alive ) {
			ret |= knights & (1ull << (pp.column + pp.row * 8) );
		}
	}
	for( unsigned char pi = pieces::pawn1; pi <= pieces::pawn8; ++pi ) {
		piece const& pp = p.pieces[1-c][pi];
		if( !pp.alive || !pp.special ) {
			continue;
		}
		unsigned short promoted = (p.promotions[1-c] >> (2 * (pi - pieces::pawn1) ) ) & 0x03;
		if( promoted == promotions::knight ) {
			ret |= knights & (1ull << (pp.column + pp.row * 8) );
		}
	}

	return ret;
}

bool detect_check_from_queen( position const& p, color::type c, unsigned char king_col, unsigned char king_row, piece const& pp, unsigned char ignore_col, unsigned char ignore_row )
{
	signed char cx = static_cast<signed char>(king_col) - pp.column;
	signed char cy = static_cast<signed char>(king_row) - pp.row;

	if( !cx ) {
		cy = (cy > 0) ? 1 : -1;

		unsigned char row;
		for( row = pp.row + cy; row != king_row; row += cy ) {
			if( king_col == ignore_col && row == ignore_row ) {
				continue;
			}
			if( p.board[king_col][row] != pieces::nil ) {
				break;
			}
		}
		if( row == king_row ) {
			return true;
		}
	}
	else if( !cy ) {
		cx = (cx > 0) ? 1 : -1;

		unsigned char col;
		for( col = pp.column + cx; col != king_col; col += cx ) {
			if( col == ignore_col && king_row == ignore_row ) {
				continue;
			}
			if( p.board[col][king_row] != pieces::nil ) {
				break;
			}
		}
		if( col == king_col ) {
			return true;
		}
	}
	else if( cx == cy || cx == -cy ) {
		cx = (cx > 0) ? 1 : -1;
		cy = (cy > 0) ? 1 : -1;

		unsigned char row;
		unsigned char col;
		for( col = pp.column + cx, row = pp.row + cy; col != king_col; col += cx, row += cy ) {
			if( col == ignore_col && row == ignore_row ) {
				continue;
			}
			if( p.board[col][row] != pieces::nil ) {
				break;
			}
		}
		if( row == king_row ) {
			return true;
		}
	}

	return false;
}


bool detect_check_from_queen( position const& p, color::type c, unsigned char king_col, unsigned char king_row, unsigned char ignore_col, unsigned char ignore_row )
{
	piece const& pp = p.pieces[1-c][pieces::queen];
	if( pp.alive ) {
		return detect_check_from_queen( p, c, king_col, king_row, pp, ignore_col, ignore_row );
	}

	return false;
}


bool detect_check_from_rook( position const& p, color::type c, unsigned char king_col, unsigned char king_row, piece const& pp, unsigned char ignore_col, unsigned char ignore_row )
{
	signed char cx = static_cast<signed char>(king_col) - pp.column;
	signed char cy = static_cast<signed char>(king_row) - pp.row;

	if( !cx ) {
		cy = (cy > 0) ? 1 : -1;

		unsigned char row;
		for( row = pp.row + cy; row != king_row; row += cy ) {
			if( king_col == ignore_col && row == ignore_row ) {
				continue;
			}
			if( p.board[king_col][row] != pieces::nil ) {
				break;
			}
		}
		if( row == king_row ) {
			return true;
		}
	}
	else if( !cy ) {
		cx = (cx > 0) ? 1 : -1;

		unsigned char col;
		for( col = pp.column + cx; col != king_col; col += cx ) {
			if( col == ignore_col && king_row == ignore_row ) {
				continue;
			}
			if( p.board[col][king_row] != pieces::nil ) {
				break;
			}
		}
		if( col == king_col ) {
			return true;
		}
	}

	return false;
}


bool detect_check_from_rooks( position const& p, color::type c, unsigned char king_col, unsigned char king_row, unsigned char ignore_col, unsigned char ignore_row )
{
	{
		piece const& pp = p.pieces[1-c][pieces::rook1];
		if( pp.alive ) {
			if( detect_check_from_rook( p, c, king_col, king_row, pp, ignore_col, ignore_row ) ) {
				return true;
			}
		}
	}
	{
		piece const& pp = p.pieces[1-c][pieces::rook2];
		if( pp.alive ) {
			if( detect_check_from_rook( p, c, king_col, king_row, pp, ignore_col, ignore_row ) ) {
				return true;
			}
		}
	}

	return false;
}


bool detect_check_from_bishop( position const& p, color::type c, unsigned char king_col, unsigned char king_row, piece const& pp, unsigned char ignore_col, unsigned char ignore_row )
{
	signed char cx = static_cast<signed char>(king_col) - pp.column;
	signed char cy = static_cast<signed char>(king_row) - pp.row;

	if( cx == cy || cx == -cy ) {
		cx = (cx > 0) ? 1 : -1;
		cy = (cy > 0) ? 1 : -1;

		unsigned char row;
		unsigned char col;
		for( col = pp.column + cx, row = pp.row + cy; col != king_col; col += cx, row += cy ) {
			if( col == ignore_col && row == ignore_row ) {
				continue;
			}
			if( p.board[col][row] != pieces::nil ) {
				break;
			}
		}
		if( row == king_row ) {
			return true;
		}
	}

	return false;
}


bool detect_check_from_bishops( position const& p, color::type c, unsigned char king_col, unsigned char king_row, unsigned char ignore_col, unsigned char ignore_row )
{
	{
		piece const& pp = p.pieces[1-c][pieces::bishop1];
		if( pp.alive ) {
			if( detect_check_from_bishop( p, c, king_col, king_row, pp, ignore_col, ignore_row ) ) {
				return true;
			}
		}
	}
	{
		piece const& pp = p.pieces[1-c][pieces::bishop2];
		if( pp.alive ) {
			if( detect_check_from_bishop( p, c, king_col, king_row, pp, ignore_col, ignore_row ) ) {
				return true;
			}
		}
	}

	return false;
}


bool detect_check_from_pawn( position const& p, color::type c, unsigned char king_col, unsigned char king_row, piece const& pp )
{
	if( c == color::white ) {
		if( pp.row != king_row + 1 ) {
			return false;
		}
	}
	else {
		if( pp.row + 1 != king_row ) {
			return false;
		}
	}

	signed char cx = static_cast<signed char>(king_col) - pp.column;
	if( cx != 1 && cx != -1 ) {
		return false;
	}

	return true;
}

bool detect_check_from_pawns( position const& p, color::type c, unsigned char king_col, unsigned char king_row, unsigned char ignore_col, unsigned char ignore_row )
{
	for( unsigned char i = pieces::pawn1; i <= pieces::pawn8; ++i ) {
		piece const& pp = p.pieces[1-c][i];
		if( pp.alive ) {
			if( pp.special ) {
				unsigned char promoted = ( p.promotions[1-c] >> (2 * (i - pieces::pawn1) ) ) & 0x03;
				switch( promoted ) {
				case promotions::queen:
					if( detect_check_from_queen( p, c, king_col, king_row, pp, ignore_col, ignore_row ) ) {
						return true;
					}
					break;
				case promotions::rook:
					if( detect_check_from_rook( p, c, king_col, king_row, pp, ignore_col, ignore_row ) ) {
						return true;
					}
					break;
				case promotions::bishop:
					if( detect_check_from_bishop( p, c, king_col, king_row, pp, ignore_col, ignore_row ) ) {
						return true;
					}
					break;
				default:
					// Knights are handled differently
					break;
				}
			}
			else {
				if( detect_check_from_pawn( p, c, king_col, king_row, pp ) ) {
					return true;
				}
			}
		}
	}
	return false;
}


bool detect_check( position const& p, color::type c, unsigned char king_col, unsigned char king_row, unsigned char ignore_col, unsigned char ignore_row )
{
	return 	detect_check_from_pawns( p, c, king_col, king_row, ignore_col, ignore_row ) ||
			detect_check_from_queen( p, c, king_col, king_row, ignore_col, ignore_row ) ||
			detect_check_from_rooks( p, c, king_col, king_row, ignore_col, ignore_row ) ||
			detect_check_from_bishops( p, c, king_col, king_row, ignore_col, ignore_row ) ||
			detect_check_knights( p, c, king_col, king_row );
}

bool detect_check( position const& p, color::type c )
{
	unsigned char king_col = p.pieces[c][pieces::king].column;
	unsigned char king_row = p.pieces[c][pieces::king].row;

	return detect_check( p, c, king_col, king_row, king_col, king_row );
}


void calc_check_map_knight( position const& p, color::type c, check_map& map, signed char king_col, signed char king_row, int col, int row )
{
	unsigned char index = p.board[col][row];
	if( index == pieces::nil ) {
		return;
	}


	unsigned char piece_color = (index >> 4) & 0x1;
	if( piece_color == c ) {
		return;
	}

	// Enemy piece
	unsigned char pi = index & 0x0f;

	bool check = false;
	if( pi == pieces::knight1 || pi == pieces::knight2 ) {
		check = true;
	}
	else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
		// Check for promoted queens
		piece const& pp = p.pieces[1 - c][pi];
		if( pp.special ) {
			unsigned short promoted = (p.promotions[1 - c] >> ((pi - pieces::pawn1) * 2) ) & 0x03;
			if( promoted == promotions::knight ) {
				check = true;
			}
		}
	}

	if( check ) {
		unsigned char v = 0x80 | (col << 3) | row;
		map.board[col][row] = v;

		if( !map.board[king_col][king_row] ) {
			map.board[king_col][king_row] = v;
		}
		else {
			map.board[king_col][king_row] = 0x80 | 0x40;
		}
	}
}

void calc_check_map( position const& p, color::type c, check_map& map )
{
	// Only thing this function is missing is a goto 10
	memset( &map, 0, sizeof(check_map) );

	signed char king_col = p.pieces[c][pieces::king].column;
	signed char king_row = p.pieces[c][pieces::king].row;

	// Check diagonals
	signed char col, row;
	for( signed char cx = -1; cx <= 1; cx += 2 ) {
		for( signed char cy = -1; cy <= 1; cy += 2 ) {

			bool found_own = false;

			for( col = king_col + cx, row = king_row + cy;
				   col >= 0 && col < 8 && row >= 0 && row < 8; col += cx, row += cy ) {

				unsigned char index = p.board[col][row];
				if( index == pieces::nil ) {
					continue;
				}

				unsigned char piece_color = (index >> 4) & 0x1;
				if( piece_color == c ) {
					if( found_own ) {
						break; // Two own pieces blocking check
					}
					found_own = true;
					continue;
				}

				unsigned char pi = index & 0x0f;

				// Enemy piece
				bool check = false;
				if( pi == pieces::queen || pi == pieces::bishop1 || pi == pieces::bishop2 ) {
					check = true;
				}
				else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
					// Check for promoted queens
					piece const& pp = p.pieces[1 - c][pi];
					if( pp.special ) {
						unsigned short promoted = (p.promotions[1 - c] >> ((pi - pieces::pawn1) * 2) ) & 0x03;
						if( promoted == promotions::queen || promoted == promotions::bishop ) {
							check = true;
						}
					}
					else if( c && static_cast<signed char>(king_row) == (row + 1) ) {
						// The pawn itself giving chess
						check = true;
					}
					else if( !c && row == (static_cast<signed char>(king_row) + 1) ) {
						// The pawn itself giving chess
						check = true;
					}
				}

				if( check ) {
					// Backtrack
					unsigned char v = 0x80 | (col << 3) | row;
					signed char bcol = col;
					signed char brow = row;
					for( ; bcol != king_col && brow != king_row; bcol -= cx, brow -= cy ) {
						map.board[bcol][brow] = v;
					}
					if( !found_own ) {
						if( !map.board[king_col][king_row] ) {
							map.board[king_col][king_row] = v;
						}
						else {
							map.board[king_col][king_row] = 0x80 | 0x40;
						}
					}
				}

				break;
			}
		}
	}

	// Check horizontals
	for( signed char cx = -1; cx <= 1; cx += 2 ) {
		bool found_own = false;

		for( col = king_col + cx; col >= 0 && col < 8; col += cx ) {

			unsigned char index = p.board[col][king_row];
			if( index == pieces::nil ) {
				continue;
			}

			unsigned char piece_color = (index >> 4) & 0x1;
			if( piece_color == c ) {
				if( found_own ) {
					break; // Two own pieces blocking check
				}
				found_own = true;
				continue;
			}

			unsigned char pi = index & 0x0f;

			// Enemy piece
			bool check = false;
			if( pi == pieces::queen || pi == pieces::rook1 || pi == pieces::rook2 ) {
				check = true;
			}
			else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
				// Check for promoted queens
				piece const& pp = p.pieces[1 - c][pi];
				if( pp.special ) {
					unsigned short promoted = (p.promotions[1 - c] >> ((pi - pieces::pawn1) * 2) ) & 0x03;
					if( promoted == promotions::queen || promoted == promotions::rook ) {
						check = true;
					}
				}
			}

			if( check ) {
				// Backtrack
				unsigned char v = 0x80 | (col << 3) | king_row;
				signed char bcol = col;
				for( ; bcol != king_col; bcol -= cx ) {
					map.board[bcol][king_row] = v;
				}
				if( !found_own ) {
					if( !map.board[king_col][king_row] ) {
						map.board[king_col][king_row] = v;
					}
					else {
						map.board[king_col][king_row] = 0x80 | 0x40;
					}
				}
			}

			break;
		}
	}

	// Check verticals
	for( signed char cy = -1; cy <= 1; cy += 2 ) {

		bool found_own = false;

		for( row = king_row + cy; row >= 0 && row < 8; row += cy ) {

			unsigned char index = p.board[king_col][row];
			if( index == pieces::nil ) {
				continue;
			}

			unsigned char piece_color = (index >> 4) & 0x1;
			if( piece_color == c ) {
				if( found_own ) {
					break; // Two own pieces blocking check
				}
				found_own = true;
				continue;
			}

			unsigned char pi = index & 0x0f;

			// Enemy piece
			bool check = false;
			if( pi == pieces::queen || pi == pieces::rook1 || pi == pieces::rook2 ) {
				check = true;
			}
			else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
				// Check for promoted queens
				piece const& pp = p.pieces[1 - c][pi];
				if( pp.special ) {
					unsigned short promoted = (p.promotions[1 - c] >> ((pi - pieces::pawn1) * 2) ) & 0x03;
					if( promoted == promotions::queen || promoted == promotions::rook ) {
						check = true;
					}
				}
			}

			if( check ) {
				// Backtrack
				unsigned char v = 0x80 | (king_col << 3) | row;
				signed char brow = row;
				for( ; brow != king_row; brow -= cy ) {
					map.board[king_col][brow] = v;
				}
				if( !found_own ) {
					if( !map.board[king_col][king_row] ) {
						map.board[king_col][king_row] = v;
					}
					else {
						map.board[king_col][king_row] = 0x80 | 0x40;
					}
				}
			}

			break;
		}
	}

	unsigned long long knights = possible_knight_moves[king_col + king_row * 8];
	unsigned long long knight;
	while( knights ) {
		bitscan( knights, knight );
		knights ^= 1ull << knight;
		calc_check_map_knight( p, c, map, king_col, king_row, knight & 0x7, knight >> 3 );
	}

	unsigned char cv = map.board[king_col][king_row];
	map.check = cv;
}


void calc_inverse_check_map( position const& p, color::type c, inverse_check_map& map )
{
	// Only thing this function is missing is a goto 10
	memset( &map, 0, sizeof(inverse_check_map) );

	signed char king_col = p.pieces[1-c][pieces::king].column;
	signed char king_row = p.pieces[1-c][pieces::king].row;

	// Check diagonals
	signed char col, row;
	for( signed char cx = -1; cx <= 1; cx += 2 ) {
		for( signed char cy = -1; cy <= 1; cy += 2 ) {

			signed char own_col = 8;
			signed char own_row = 8;

			for( col = king_col + cx, row = king_row + cy;
				   col >= 0 && col < 8 && row >= 0 && row < 8; col += cx, row += cy ) {

				unsigned char index = p.board[col][row];
				if( index == pieces::nil ) {
					if( own_col == 8 ) {
						map.board[col][row] = 0xc0;
					}
					continue;
				}

				unsigned char piece_color = (index >> 4) & 0x1;
				if( piece_color != c ) {
					// Enemy piece in direct line of sight. Capture to check.
					map.board[col][row] = 0xc0;
					break;
				}

				if( own_col == 8 ) {
					own_col = col;
					own_row = row;
					continue;
				}

				unsigned char pi = index & 0x0f;

				// Own piece on enemy king ray blocked by one other own piece
				bool blocked_check = false;
				if( pi == pieces::queen || pi == pieces::bishop1 || pi == pieces::bishop2 ) {
					blocked_check = true;
				}
				else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
					// Check for promoted queens
					piece const& pp = p.pieces[c][pi];
					if( pp.special ) {
						unsigned short promoted = (p.promotions[c] >> ((pi - pieces::pawn1) * 2) ) & 0x03;
						if( promoted == promotions::queen || promoted == promotions::bishop ) {
							blocked_check = true;
						}
					}
				}

				if( blocked_check ) {
					unsigned char v = 0x80 | (col << 3) | row;
					map.board[own_col][own_row] = v;
				}

				break;
			}
		}
	}

	// Check horizontals
	for( signed char cx = -1; cx <= 1; cx += 2 ) {

		signed char own_col = 8;

		for( col = king_col + cx; col >= 0 && col < 8; col += cx ) {

			unsigned char index = p.board[col][king_row];
			if( index == pieces::nil ) {
				if( own_col == 8 ) {
					map.board[col][king_row] = 0x80;
				}
				continue;
			}

			unsigned char piece_color = (index >> 4) & 0x1;
			if( piece_color != c ) {
				// Enemy piece in direct line of sight. Capture to check.
				map.board[col][king_row] = 0x80;
				break;
			}

			if( own_col == 8 ) {
				own_col = col;
				continue;
			}

			unsigned char pi = index & 0x0f;

			// Own piece on enemy king ray blocked by one other own piece
			bool blocked_check = false;
			if( pi == pieces::queen || pi == pieces::rook1 || pi == pieces::rook2 ) {
				blocked_check = true;
			}
			else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
				// Check for promoted queens
				piece const& pp = p.pieces[c][pi];
				if( pp.special ) {
					unsigned short promoted = (p.promotions[c] >> ((pi - pieces::pawn1) * 2) ) & 0x03;
					if( promoted == promotions::queen || promoted == promotions::rook ) {
						blocked_check = true;
					}
				}
			}

			if( blocked_check ) {
				unsigned char v = 0x80 | (col << 3) | king_row;
				map.board[own_col][king_row] = v;
			}

			break;
		}
	}

	// Check verticals
	for( signed char cy = -1; cy <= 1; cy += 2 ) {

		signed char own_row = 8;

		for( row = king_row + cy; row >= 0 && row < 8; row += cy ) {

			unsigned char index = p.board[king_col][row];
			if( index == pieces::nil ) {
				if( own_row == 8 ) {
					map.board[king_col][row] = 0x80;
				}
				continue;
			}

			unsigned char piece_color = (index >> 4) & 0x1;
			if( piece_color != c ) {
				// Enemy piece in direct line of sight. Capture to check.
				map.board[king_col][row] = 0x80;
				break;
			}

			if( own_row == 8 ) {
				own_row = row;
				continue;
			}

			unsigned char pi = index & 0x0f;

			// Own piece on enemy king ray blocked by one other own piece
			bool blocked_check = false;
			if( pi == pieces::queen || pi == pieces::rook1 || pi == pieces::rook2 ) {
				blocked_check = true;
			}
			else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
				// Check for promoted queens
				piece const& pp = p.pieces[c][pi];
				if( pp.special ) {
					unsigned short promoted = (p.promotions[c] >> ((pi - pieces::pawn1) * 2) ) & 0x03;
					if( promoted == promotions::queen || promoted == promotions::rook ) {
						blocked_check = true;
					}
				}
			}

			if( blocked_check ) {
				unsigned char v = 0x80 | (row << 3) | king_col;
				map.board[king_col][own_row] = v;
			}

			break;
		}
	}
}
