#include "chess.hpp"
#include "detect_check.hpp"
#include "sliding_piece_attacks.hpp"
#include "magic.hpp"
#include "tables.hpp"

bool detect_check_knights( position const& p, color::type c, unsigned char king )
{
	unsigned long long knights = possible_knight_moves[ king ];
	knights &= p.bitboards[1-c].b[bb_type::knights];

	return knights != 0;
}

bool detect_check( position const& p, color::type c, unsigned char king, unsigned char ignore )
{
	unsigned long long blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];
	blockers &= ~(1ull << ignore);

	unsigned long long rooks_and_queens = p.bitboards[1-c].b[bb_type::rooks] | p.bitboards[1-c].b[bb_type::queens];
	unsigned long long bishops_and_queens = p.bitboards[1-c].b[bb_type::bishops] | p.bitboards[1-c].b[bb_type::queens];
	bishops_and_queens |= pawn_control[c][king] & p.bitboards[1-c].b[bb_type::pawns];

	unsigned long long rooks_and_queens_check = rook_magic( king, blockers ) & rooks_and_queens;
	unsigned long long bishops_and_queens_check = bishop_magic( king, blockers ) & bishops_and_queens;

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
