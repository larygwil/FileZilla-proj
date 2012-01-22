#include "chess.hpp"
#include "detect_check.hpp"
#include "sliding_piece_attacks.hpp"

extern unsigned long long const pawn_control[2][64];

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

bool detect_check_knights( position const& p, color::type c, unsigned char king )
{
	unsigned long long knights = possible_knight_moves[ king ];
	knights &= p.bitboards[1-c].b[bb_type::knights];

	return knights != 0;
}

#include <iostream>
bool detect_check( position const& p, color::type c, unsigned char king, unsigned char ignore )
{
	unsigned long long blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];
	blockers &= ~(1ull << ignore);

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

	return rooks_and_queens_check || bishops_and_queens_check || detect_check_knights( p, c, king );
}

bool detect_check( position const& p, color::type c )
{
	unsigned long long kings = p.bitboards[c].b[bb_type::king];
	unsigned long long king;
	bitscan( kings, king );

	return detect_check( p, c, king, king );
}


void calc_check_map_knight( check_map& map, unsigned char king, unsigned char knight )
{
	unsigned char v = 0x80 | knight;
	map.board[knight] = v;

	if( !map.board[king] ) {
		map.board[king] = v;
	}
	else {
		map.board[king] = 0x80 | 0x40;
	}
}


namespace {
static void process_ray( position const& p, color::type c, check_map& map, unsigned long long ray, unsigned long long potential_check_givers, unsigned char king )
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

				map.board[pi] = cpi;
			}

			if( !block_count ) {
				if( !map.board[king] ) {
					map.board[king] = cpi;
				}
				else {
					map.board[king] = 0x80 | 0x40;
				}
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
	process_ray( p, c, map, unblocked_king_n, rooks_and_queens, king );
	process_ray( p, c, map, unblocked_king_e, rooks_and_queens, king );
	process_ray( p, c, map, unblocked_king_s, rooks_and_queens, king );
	process_ray( p, c, map, unblocked_king_w, rooks_and_queens, king );

	unsigned long long bishops_and_queens = p.bitboards[1-c].b[bb_type::bishops] | p.bitboards[1-c].b[bb_type::queens];
	bishops_and_queens |= pawn_control[c][king] & p.bitboards[1-c].b[bb_type::pawns];
	process_ray( p, c, map, unblocked_king_ne, bishops_and_queens, king );
	process_ray( p, c, map, unblocked_king_se, bishops_and_queens, king );
	process_ray( p, c, map, unblocked_king_sw, bishops_and_queens, king );
	process_ray( p, c, map, unblocked_king_nw, bishops_and_queens, king );

	unsigned long long knights = possible_knight_moves[king] & p.bitboards[1-c].b[bb_type::knights];
	unsigned long long knight;
	while( knights ) {
		bitscan( knights, knight );
		knights &= knights - 1;
		calc_check_map_knight( map, king, knight );
	}

	unsigned char cv = map.board[king];
	map.check = cv;
}


namespace {
static void process_ray_inverse( inverse_check_map& map, unsigned long long ray, unsigned long long ignore, unsigned long long potential_check_givers )
{
	potential_check_givers &= ray;
	unsigned long long blockers = ray & ignore;

	if( potential_check_givers && blockers ) {
		unsigned long long blocker;
		bitscan( blockers, blocker );

		unsigned long long cpi;
		bitscan( potential_check_givers, cpi );

		map.board[blocker] = cpi | 0x80;
	}
}
}


void calc_inverse_check_map( position const& p, color::type c, inverse_check_map& map )
{
	memset( &map, 0, sizeof(inverse_check_map) );

	unsigned long long kings = p.bitboards[1-c].b[bb_type::king];
	unsigned long long king;
	bitscan( kings, king );

	map.enemy_king_col = static_cast<unsigned char>(king % 8);
	map.enemy_king_row = static_cast<unsigned char>(king / 8);

	unsigned long long blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];
	unsigned long long blocked_king_n = attack( king, blockers, ray_n );
	unsigned long long blocked_king_e = attack( king, blockers, ray_e );
	unsigned long long blocked_king_s = attackr( king, blockers, ray_s );
	unsigned long long blocked_king_w = attackr( king, blockers, ray_w );
	unsigned long long blocked_king_ne = attack( king, blockers, ray_ne );
	unsigned long long blocked_king_se = attackr( king, blockers, ray_se );
	unsigned long long blocked_king_sw = attackr( king, blockers, ray_sw );
	unsigned long long blocked_king_nw = attack( king, blockers, ray_nw );

	unsigned long long rooks = blocked_king_n | blocked_king_e | blocked_king_s | blocked_king_w;
	unsigned long long bishops = blocked_king_ne | blocked_king_se | blocked_king_sw | blocked_king_nw;
	unsigned long long all = rooks | bishops;

	unsigned long long ignore = p.bitboards[c].b[bb_type::all_pieces] & all;

	unsigned long long blockers2 = blockers & ~(ignore);
	unsigned long long unblocked_king_n = attack( king, blockers2, ray_n );
	unsigned long long unblocked_king_e = attack( king, blockers2, ray_e );
	unsigned long long unblocked_king_s = attackr( king, blockers2, ray_s );
	unsigned long long unblocked_king_w = attackr( king, blockers2, ray_w );
	unsigned long long unblocked_king_ne = attack( king, blockers2, ray_ne );
	unsigned long long unblocked_king_se = attackr( king, blockers2, ray_se );
	unsigned long long unblocked_king_sw = attackr( king, blockers2, ray_sw );
	unsigned long long unblocked_king_nw = attack( king, blockers2, ray_nw );

	unsigned long long rooks_and_queens = p.bitboards[c].b[bb_type::rooks] | p.bitboards[c].b[bb_type::queens];
	process_ray_inverse( map, unblocked_king_n, ignore, rooks_and_queens );
	process_ray_inverse( map, unblocked_king_e, ignore, rooks_and_queens );
	process_ray_inverse( map, unblocked_king_s, ignore, rooks_and_queens );
	process_ray_inverse( map, unblocked_king_w, ignore, rooks_and_queens );

	unsigned long long bishops_and_queens = p.bitboards[c].b[bb_type::bishops] | p.bitboards[c].b[bb_type::queens];
	process_ray_inverse( map, unblocked_king_ne, ignore, bishops_and_queens );
	process_ray_inverse( map, unblocked_king_se, ignore, bishops_and_queens );
	process_ray_inverse( map, unblocked_king_sw, ignore, bishops_and_queens );
	process_ray_inverse( map, unblocked_king_nw, ignore, bishops_and_queens );

	// Mark empty squares
	rooks &= ~p.bitboards[c].b[bb_type::all_pieces];
	while ( rooks ) {
		unsigned long long rook;
		bitscan( rooks, rook );
		rooks &= rooks - 1;

		map.board[rook] = 0x80;
	}

	bishops &= ~p.bitboards[c].b[bb_type::all_pieces];
	while ( bishops ) {
		unsigned long long bishop;
		bitscan( bishops, bishop );
		bishops &= bishops - 1;

		map.board[bishop] = 0xc0;
	}
}
