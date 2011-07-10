#include "chess.hpp"
#include "detect_check.hpp"

// Returns false on multiple, further check testing is skipped
bool detect_check_knight( position const& p, color::type c, int column, int row, int cx, int cy, check_info& check )
{
	column += cx;
	if( column < 0 || column > 7 ) {
		return true;
	}

	row += cy;
	if( row < 0 || row > 7 ) {
		return true;
	}

	int pi = p.board[column][row];
	if( pi == pieces::nil ) {
		return true;
	}

	if( c == (pi >> 4) ) {
		// Own pieces do not check
		return true;
	}

	pi &= 0x0f;

	if( pi == pieces::knight1 || pi == pieces::knight2 ) {
		if( check.check ) {
			check.multiple = true;
			return false;
		}
		check.check = true;
		check.piece = pi;
	}
	else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
		piece const& pp = p.pieces[static_cast<color::type>(1-c)][pi];
		if( pp.special ) {
			unsigned short pknight = promotions::knight << (2 * (pi - pieces::pawn1) );
			if( (p.promotions[static_cast<color::type>(1-c)] & pknight) == pknight ) {
				if( check.check ) {
					check.multiple = true;
					return false;
				}
				check.check = true;
				check.piece = pi;
			}
		}
	}

	return true;
}

void detect_check_knight( position const& p, color::type c, check_info& check, int king_col, int king_row, piece const& pp, unsigned char pi ) {
	signed char cx = king_col - pp.column;
	if( !cx ) {
		return;
	}

	signed char cy = king_row - pp.row;
	if( !cy ) {
		return;
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
	if( sum == 3 ) {
		if( check.check ) {
			check.multiple = true;
		}
		else {
			check.check = true;
			check.piece = pi;
		}
	}
}

void detect_check_knights( position const& p, color::type c, check_info& check, int king_col, int king_row )
{
	{
		piece const& pp = p.pieces[1-c][pieces::knight1];
		if( pp.alive ) {
			detect_check_knight( p, c, check, king_col, king_row, pp, pieces::knight1 );
		}
	}
	{
		piece const& pp = p.pieces[1-c][pieces::knight2];
		if( pp.alive ) {
			detect_check_knight( p, c, check, king_col, king_row, pp, pieces::knight2 );
		}
	}
	for( unsigned char pi = pieces::pawn1; pi < pieces::pawn8; ++pi ) {
		piece const& pp = p.pieces[1-c][pieces::knight2];
		if( !pp.alive || ! pp.special ) {
			continue;
		}
		unsigned short promoted = (p.promotions[1-c] >> (2 * (pi - pieces::pawn1) ) ) & 0x03;
		if( promoted == promotions::knight ) {
			detect_check_knight( p, c, check, king_col, king_row, pp, pieces::knight2 );
		}
	}
//	if( !detect_check_knight( p, c, king_col, king_row, 1, 2, check ) ) return;
//	if( !detect_check_knight( p, c, king_col, king_row, 2, 1, check ) ) return;
//	if( !detect_check_knight( p, c, king_col, king_row, 2, -1, check ) ) return;
//	if( !detect_check_knight( p, c, king_col, king_row, 1, -2, check ) ) return;
//	if( !detect_check_knight( p, c, king_col, king_row, -1, -2, check ) ) return;
//	if( !detect_check_knight( p, c, king_col, king_row, -2, -1, check ) ) return;
//	if( !detect_check_knight( p, c, king_col, king_row, -2, 1, check ) ) return;
//	if( !detect_check_knight( p, c, king_col, king_row, -1, 2, check ) ) return;
}

#if 0
//#define SET_CHECK_RET_ON_MULTIPLE(check, pi) { if( check.check ) { check.multiple = true; return; } check.check = true; check.piece = pi; }
#define SET_CHECK_RET_ON_MULTIPLE(check, pi) { check.check = true; return; }
void detect_check( position const& p, color::type c, check_info& check, unsigned char king_col, unsigned char king_row )
{
	check.check = false;
	check.multiple = false;
	check.piece = pieces::pawn1;

	// Check diagonals
	int col, row;
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int cy = -1; cy <= 1; cy += 2 ) {
			for( col = static_cast<signed char>(king_col) + cx, row = static_cast<signed char>(king_row) + cy;
				   col >= 0 && col < 8 && row >= 0 && row < 8; col += cx, row += cy ) {

				unsigned char index = p.board[col][row];
				if( index == pieces::nil ) {
					continue;
				}

				unsigned char piece_color = (index >> 4) & 0x1;
				if( piece_color == c )
					break; // Own pieces do not check.

				unsigned char pi = index & 0x0f;

				if( pi == pieces::queen || pi == pieces::bishop1 || pi == pieces::bishop2 ) {
					SET_CHECK_RET_ON_MULTIPLE( check, pi );
				}
				else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
					// Check for promoted queens
					piece const& pp = p.pieces[1 - c][pi];
					if( pp.special ) {
						unsigned short promoted = p.promotions[1 - c] >> ((pi - pieces::pawn1) * 2);
						if( promoted == promotions::queen || promoted == promotions::bishop ) {
							SET_CHECK_RET_ON_MULTIPLE( check, pi )
						}
					}
					else if( c && static_cast<signed char>(king_row) == (row + 1) ) {
						// The pawn itself giving chess
						SET_CHECK_RET_ON_MULTIPLE( check, pi )
					}
					else if( !c && row == (static_cast<signed char>(king_row) + 1) ) {
						// The pawn itself giving chess
						SET_CHECK_RET_ON_MULTIPLE( check, pi )
					}
				}

				break;
			}
		}
	}

	// Horizontals
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( col = static_cast<signed char>(king_col) + cx;
			col >= 0 && col < 8; col += cx )
		{
			unsigned char index = p.board[col][king_row];
			if( index == pieces::nil ) {
				continue;
			}

			unsigned char piece_color = (index >> 4) & 0x1;
			if( piece_color == c )
				break; // Own pieces do not check.

			unsigned char pi = index & 0x0f;

			if( pi == pieces::queen || pi == pieces::rook1 || pi == pieces::rook2 ) {
				SET_CHECK_RET_ON_MULTIPLE( check, pi )
			}
			else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
				// Check for promoted queen and rooks
				piece const& pp = p.pieces[1 - c][pi];
				if( pp.special ) {
					unsigned short promoted = p.promotions[1 - c] >> ((pi - pieces::pawn1) * 2);
					if( promoted == promotions::queen || promoted == promotions::rook ) {
						SET_CHECK_RET_ON_MULTIPLE( check, pi )
					}
				}
			}

			break;
		}
	}

	// Verticals
	for( int cy = -1; cy <= 1; cy += 2 ) {
		for( row = static_cast<signed char>(king_row) + cy;
			row >= 0 && row < 8; row += cy )
		{
			unsigned char index = p.board[king_col][row];
			if( index == pieces::nil ) {
				continue;
			}

			unsigned char piece_color = (index >> 4) & 0x1;
			if( piece_color == c )
				break; // Own pieces do not check.

			unsigned char pi = index & 0x0f;

			if( pi == pieces::queen || pi == pieces::rook1 || pi == pieces::rook2 ) {
				SET_CHECK_RET_ON_MULTIPLE( check, pi )
			}
			else if( pi >= pieces::pawn1 && pi <= pieces::pawn8 ) {
				// Check for promoted queen and rooks
				piece const& pp = p.pieces[1 - c][pi];
				if( pp.special ) {
					unsigned short promoted = p.promotions[1 - c] >> ((pi - pieces::pawn1) * 2);
					if( promoted == promotions::queen || promoted == promotions::rook ) {
						SET_CHECK_RET_ON_MULTIPLE( check, pi )
					}
				}
			}

			break;
		}
	}

	detect_check_knights( p, c, check, king_col, king_row );
}
#else

#define SET_CHECK_RET_MULTIPLE(check) { if( check.check ) { check.multiple = true; } else {check.check = true; } }

void detect_check_from_queen( position const& p, color::type c, check_info& check, unsigned char king_col, unsigned char king_row, piece const& pp )
{
	signed char cx = static_cast<signed char>(king_col) - pp.column;
	signed char cy = static_cast<signed char>(king_row) - pp.row;

	if( !cx ) {
		cy = (cy > 0) ? 1 : -1;

		unsigned char row;
		for( row = pp.row + cy; row != king_row; row += cy ) {
			if( p.board[king_col][row] != pieces::nil ) {
				break;
			}
		}
		if( row == king_row ) {
			SET_CHECK_RET_MULTIPLE( check )
		}
	}
	else if( !cy ) {
		cx = (cx > 0) ? 1 : -1;

		unsigned char col;
		for( col = pp.column + cx; col != king_col; col += cx ) {
			if( p.board[col][king_row] != pieces::nil ) {
				break;
			}
		}
		if( col == king_col ) {
			SET_CHECK_RET_MULTIPLE( check )
		}
	}
	else if( cx == cy || cx == -cy ) {
		cx = (cx > 0) ? 1 : -1;
		cy = (cy > 0) ? 1 : -1;

		unsigned char row;
		unsigned char col;
		for( col = pp.column + cx, row = pp.row + cy; col != king_col; col += cx, row += cy ) {
			if( p.board[col][row] != pieces::nil ) {
				break;
			}
		}
		if( row == king_row ) {
			SET_CHECK_RET_MULTIPLE( check )
		}
	}
}


void detect_check_from_queen( position const& p, color::type c, check_info& check, unsigned char king_col, unsigned char king_row )
{
	piece const& pp = p.pieces[1-c][pieces::queen];
	if( pp.alive ) {
		detect_check_from_queen( p, c, check, king_col, king_row, pp );
	}
}


void detect_check_from_rook( position const& p, color::type c, check_info& check, unsigned char king_col, unsigned char king_row, piece const& pp )
{
	signed char cx = static_cast<signed char>(king_col) - pp.column;
	signed char cy = static_cast<signed char>(king_row) - pp.row;

	if( !cx ) {
		cy = (cy > 0) ? 1 : -1;

		unsigned char row;
		for( row = pp.row + cy; row != king_row; row += cy ) {
			if( p.board[king_col][row] != pieces::nil ) {
				break;
			}
		}
		if( row == king_row ) {
			SET_CHECK_RET_MULTIPLE( check )
		}
	}
	else if( !cy ) {
		cx = (cx > 0) ? 1 : -1;

		unsigned char col;
		for( col = pp.column + cx; col != king_col; col += cx ) {
			if( p.board[col][king_row] != pieces::nil ) {
				break;
			}
		}
		if( col == king_col ) {
			SET_CHECK_RET_MULTIPLE( check )
		}
	}
}


void detect_check_from_rooks( position const& p, color::type c, check_info& check, unsigned char king_col, unsigned char king_row )
{
	{
		piece const& pp = p.pieces[1-c][pieces::rook1];
		if( pp.alive ) {
			detect_check_from_rook( p, c, check, king_col, king_row, pp );
		}
	}
	{
		piece const& pp = p.pieces[1-c][pieces::rook2];
		if( pp.alive ) {
			detect_check_from_rook( p, c, check, king_col, king_row, pp );
		}
	}
}


void detect_check_from_bishop( position const& p, color::type c, check_info& check, unsigned char king_col, unsigned char king_row, piece const& pp )
{
	signed char cx = static_cast<signed char>(king_col) - pp.column;
	signed char cy = static_cast<signed char>(king_row) - pp.row;

	if( cx == cy || cx == -cy ) {
		cx = (cx > 0) ? 1 : -1;
		cy = (cy > 0) ? 1 : -1;

		unsigned char row;
		unsigned char col;
		for( col = pp.column + cx, row = pp.row + cy; col != king_col; col += cx, row += cy ) {
			if( p.board[col][row] != pieces::nil ) {
				break;
			}
		}
		if( row == king_row ) {
			SET_CHECK_RET_MULTIPLE( check )
		}
	}
}


void detect_check_from_bishops( position const& p, color::type c, check_info& check, unsigned char king_col, unsigned char king_row )
{
	{
		piece const& pp = p.pieces[1-c][pieces::bishop1];
		if( pp.alive ) {
			detect_check_from_bishop( p, c, check, king_col, king_row, pp );
		}
	}
	{
		piece const& pp = p.pieces[1-c][pieces::bishop2];
		if( pp.alive ) {
			detect_check_from_bishop( p, c, check, king_col, king_row, pp );
		}
	}
}


void detect_check_from_pawn( position const& p, color::type c, check_info& check, unsigned char king_col, unsigned char king_row, piece const& pp )
{
	if( c == color::white ) {
		if( pp.row != king_row + 1 ) {
			return;
		}
	}
	else {
		if( pp.row + 1 != king_row ) {
			return;
		}
	}

	signed char cx = static_cast<signed char>(king_col) - pp.column;
	if( cx != 1 && cx != -1 ) {
		return;
	}

	SET_CHECK_RET_MULTIPLE(check)
}

void detect_check_from_pawns( position const& p, color::type c, check_info& check, unsigned char king_col, unsigned char king_row )
{
	for( unsigned char i = pieces::pawn1; i <= pieces::pawn8; ++i ) {
		piece const& pp = p.pieces[1-c][i];
		if( pp.alive ) {
			if( pp.special ) {
				unsigned char promoted = ( p.promotions[1-c] >> (2 * (i - pieces::pawn1) ) ) & 0x03;
				switch( promoted ) {
				case promotions::queen:
					detect_check_from_queen( p, c, check, king_col, king_row, pp );
					break;
				case promotions::rook:
					detect_check_from_rook( p, c, check, king_col, king_row, pp );
					break;
				case promotions::bishop:
					detect_check_from_bishop( p, c, check, king_col, king_row, pp );
					break;
				default:
					// Knights are handled differently
					break;
				}
			}
			else {
				detect_check_from_pawn( p, c, check, king_col, king_row, pp );
			}
		}
	}
}


void detect_check( position const& p, color::type c, check_info& check, unsigned char king_col, unsigned char king_row )
{
	check.check = false;
	check.multiple = false;
	check.piece = pieces::pawn1;

	detect_check_from_pawns( p, c, check, king_col, king_row );
	detect_check_from_queen( p, c, check, king_col, king_row );
	detect_check_from_rooks( p, c, check, king_col, king_row );
	detect_check_from_bishops( p, c, check, king_col, king_row );
	detect_check_knights( p, c, check, king_col, king_row );
}
#endif

void detect_check( position const& p, color::type c, check_info& check )
{
	unsigned char king_col = p.pieces[c][pieces::king].column;
	unsigned char king_row = p.pieces[c][pieces::king].row;

	detect_check( p, c, check, king_col, king_row );
}

