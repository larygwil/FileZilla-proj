#include "chess.hpp"
#include "detect_check.hpp"
#include "sliding_piece_attacks.hpp"

extern unsigned long long pawn_control[2][64];

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

bool detect_check_knights( position const& p, color::type c, int king_col, int king_row )
{
	unsigned long long knights = possible_knight_moves[ king_col + king_row * 8 ];
	knights &= p.bitboards[1-c].b[bb_type::knights];

	return knights != 0;
}

#include <iostream>
bool detect_check( position const& p, color::type c, unsigned char king_col, unsigned char king_row, unsigned char ignore_col, unsigned char ignore_row )
{
	unsigned long long king = king_col + king_row * 8;

	unsigned long long blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];
	blockers &= ~(1ull << (ignore_col + ignore_row * 8) );

	unsigned long long unblocked_king_n = attack( king, blockers, ray_n );
	unsigned long long unblocked_king_e = attack( king, blockers, ray_e );
	unsigned long long unblocked_king_s = attackr( king, blockers, ray_s );
	unsigned long long unblocked_king_w = attackr( king, blockers, ray_w );
	unsigned long long unblocked_king_ne = attack( king, blockers, ray_ne );
	unsigned long long unblocked_king_se = attackr( king, blockers, ray_se );
	unsigned long long unblocked_king_sw = attackr( king, blockers, ray_sw );
	unsigned long long unblocked_king_nw = attack( king, blockers, ray_nw );

	unsigned long long rooks_and_queens = p.bitboards[1-c].b[bb_type::rooks] | p.bitboards[1-c].b[bb_type::queens];
	unsigned long long bishops_and_queens = p.bitboards[1-c].b[bb_type::bishops] | p.bitboards[1-c].b[bb_type::queens];
	bishops_and_queens |= pawn_control[c][king] & p.bitboards[1-c].b[bb_type::pawns];

	unsigned long long rooks_and_queens_check = (unblocked_king_n|unblocked_king_e|unblocked_king_s|unblocked_king_w) & rooks_and_queens;
	unsigned long long bishops_and_queens_check = (unblocked_king_ne|unblocked_king_se|unblocked_king_sw|unblocked_king_nw) & bishops_and_queens;

	return rooks_and_queens_check | bishops_and_queens_check |detect_check_knights( p, c, king_col, king_row );
}

bool detect_check( position const& p, color::type c )
{
	unsigned long long kings = p.bitboards[c].b[bb_type::king];
	unsigned long long king;
	bitscan( kings, king );

	unsigned char king_col = static_cast<unsigned char>( king % 8 );
	unsigned char king_row = static_cast<unsigned char>( king / 8 );

	return detect_check( p, c, king_col, king_row, king_col, king_row );
}


void calc_check_map_knight( position const& p, color::type c, check_map& map, signed char king_col, signed char king_row, int col, int row )
{
	unsigned char v = 0x80 | (row << 3) | col;
	map.board[col][row] = v;

	if( !map.board[king_col][king_row] ) {
		map.board[king_col][king_row] = v;
	}
	else {
		map.board[king_col][king_row] = 0x80 | 0x40;
	}
}


void process_ray( position const& p, color::type c, check_map& map, unsigned long long ray, unsigned long long potential_check_givers, unsigned char king_col, unsigned char king_row )
{
	potential_check_givers &= ray;

	if( potential_check_givers ) {
		unsigned long long block_count = popcount( ray & p.bitboards[c].b[bb_type::all_pieces] );
		if( block_count < 2 ) {
			unsigned long long cpi;
			bitscan( potential_check_givers, cpi );
			cpi |= 0x80;

			while( ray ) {
				unsigned long long pi;
				bitscan( ray, pi );
				ray &= ray - 1;

				map.board[pi % 8][pi / 8] = cpi;
			}

			if( !block_count ) {
				if( !map.board[king_col][king_row] ) {
					map.board[king_col][king_row] = cpi;
				}
				else {
					map.board[king_col][king_row] = 0x80 | 0x40;
				}
			}
		}
	}
}


void calc_check_map( position const& p, color::type c, check_map& map )
{
	memset( &map, 0, sizeof(check_map) );

	unsigned long long kings = p.bitboards[c].b[bb_type::king];
	unsigned long long king;
	bitscan( kings, king );

	signed char king_col = static_cast<signed char>( king % 8 );
	signed char king_row = static_cast<signed char>( king / 8 );

	unsigned long long blockers = p.bitboards[1-c].b[bb_type::all_pieces];
	unsigned long long unblocked_king_n = attack( king, blockers, ray_n );
	unsigned long long unblocked_king_e = attack( king, blockers, ray_e );
	unsigned long long unblocked_king_s = attackr( king, blockers, ray_s );
	unsigned long long unblocked_king_w = attackr( king, blockers, ray_w );
	unsigned long long unblocked_king_ne = attack( king, blockers, ray_ne );
	unsigned long long unblocked_king_se = attackr( king, blockers, ray_se );
	unsigned long long unblocked_king_sw = attackr( king, blockers, ray_sw );
	unsigned long long unblocked_king_nw = attack( king, blockers, ray_nw );

	unsigned long long rooks_and_queens = p.bitboards[1-c].b[bb_type::rooks] | p.bitboards[1-c].b[bb_type::queens];
	process_ray( p, c, map, unblocked_king_n, rooks_and_queens, king_col, king_row );
	process_ray( p, c, map, unblocked_king_s, rooks_and_queens, king_col, king_row );
	process_ray( p, c, map, unblocked_king_e, rooks_and_queens, king_col, king_row );
	process_ray( p, c, map, unblocked_king_w, rooks_and_queens, king_col, king_row );

	unsigned long long bishops_and_queens = p.bitboards[1-c].b[bb_type::bishops] | p.bitboards[1-c].b[bb_type::queens];
	bishops_and_queens |= pawn_control[c][king] & p.bitboards[1-c].b[bb_type::pawns];
	process_ray( p, c, map, unblocked_king_ne, bishops_and_queens, king_col, king_row );
	process_ray( p, c, map, unblocked_king_se, bishops_and_queens, king_col, king_row );
	process_ray( p, c, map, unblocked_king_sw, bishops_and_queens, king_col, king_row );
	process_ray( p, c, map, unblocked_king_nw, bishops_and_queens, king_col, king_row );

	unsigned long long knights = possible_knight_moves[king_col + king_row * 8] & p.bitboards[1-c].b[bb_type::knights];
	unsigned long long knight;
	while( knights ) {
		bitscan( knights, knight );
		knights &= knights - 1;
		calc_check_map_knight( p, c, map, king_col, king_row, static_cast<int>(knight % 8), static_cast<int>(knight / 8) );
	}

	unsigned char cv = map.board[king_col][king_row];
	map.check = cv;
}


void calc_inverse_check_map( position const& p, color::type c, inverse_check_map& map )
{
	memset( &map, 0, sizeof(inverse_check_map) );

	unsigned long long kings = p.bitboards[1-c].b[bb_type::king];
	unsigned long long king;
	bitscan( kings, king );

	signed char king_col = static_cast<signed char>( king % 8 );
	signed char king_row = static_cast<signed char>( king / 8 );
	map.enemy_king_col = static_cast<unsigned char>(king_col);
	map.enemy_king_row = static_cast<unsigned char>(king_row);

	// Check diagonals
	signed char col, row;
	for( signed char cx = -1; cx <= 1; cx += 2 ) {
		for( signed char cy = -1; cy <= 1; cy += 2 ) {

			signed char own_col = 8;
			signed char own_row = 8;

			for( col = king_col + cx, row = king_row + cy;
				   col >= 0 && col < 8 && row >= 0 && row < 8; col += cx, row += cy ) {

				unsigned char index = p.board[col][row];
				if( !index ) {
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

				pieces::type pi = static_cast<pieces::type>(index & 0x0f);

				if( pi == pieces::queen || pi == pieces::bishop ) {
					// Own piece on enemy king ray blocked by one other own piece
					unsigned char v = 0x80 | (row << 3) | col;
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
			if( !index ) {
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

			pieces::type pi = static_cast<pieces::type>(index & 0x0f);

			// Own piece on enemy king ray blocked by one other own piece
			if( pi == pieces::queen || pi == pieces::rook ) {
				unsigned char v = 0x80 | (king_row << 3) | col;
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
			if( !index ) {
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

			pieces::type pi = static_cast<pieces::type>(index & 0x0f);

			// Own piece on enemy king ray blocked by one other own piece
			if( pi == pieces::queen || pi == pieces::rook ) {
				unsigned char v = 0x80 | (row << 3) | king_col;
				map.board[king_col][own_row] = v;
			}

			break;
		}
	}
}
