#include "chess.hpp"
#include "detect_check.hpp"
#include "sliding_piece_attacks.hpp"
#include "magic.hpp"
#include "tables.hpp"

bool detect_check_knights( position const& p, color::type c, unsigned char king )
{
	uint64_t knights = possible_knight_moves[ king ];
	knights &= p.bitboards[1-c].b[bb_type::knights];

	return knights != 0;
}

bool detect_check( position const& p, color::type c, unsigned char king, unsigned char ignore )
{
	uint64_t blockers = p.bitboards[1-c].b[bb_type::all_pieces] | p.bitboards[c].b[bb_type::all_pieces];
	blockers &= ~(1ull << ignore);

	uint64_t rooks_and_queens = p.bitboards[1-c].b[bb_type::rooks] | p.bitboards[1-c].b[bb_type::queens];
	uint64_t bishops_and_queens = p.bitboards[1-c].b[bb_type::bishops] | p.bitboards[1-c].b[bb_type::queens];
	bishops_and_queens |= pawn_control[c][king] & p.bitboards[1-c].b[bb_type::pawns];

	uint64_t rooks_and_queens_check = rook_magic( king, blockers ) & rooks_and_queens;
	uint64_t bishops_and_queens_check = bishop_magic( king, blockers ) & bishops_and_queens;

	return rooks_and_queens_check || bishops_and_queens_check || detect_check_knights( p, c, king );
}

bool detect_check( position const& p, color::type c )
{
	uint64_t kings = p.bitboards[c].b[bb_type::king];
	uint64_t king = bitscan( kings );

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
static void process_ray( position const& p, color::type c, check_map& map, uint64_t ray, uint64_t potential_check_givers, unsigned char king )
{
	potential_check_givers &= ray;

	if( potential_check_givers ) {
		uint64_t block_count = popcount( ray & p.bitboards[c].b[bb_type::all_pieces] );
		if( block_count < 2 ) {
			uint64_t cpi = bitscan( potential_check_givers );
			cpi |= 0x80;

			while( ray ) {
				uint64_t pi = bitscan_unset( ray );
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


check_map::check_map( position const& p, color::type c )
{
	memset( board, 0, sizeof(board) );

	uint64_t kings = p.bitboards[c].b[bb_type::king];
	uint64_t king = bitscan( kings );

	uint64_t blockers = p.bitboards[1-c].b[bb_type::all_pieces];
	uint64_t unblocked_king_n = attack( king, blockers, ray_n );
	uint64_t unblocked_king_e = attack( king, blockers, ray_e );
	uint64_t unblocked_king_s = attackr( king, blockers, ray_s );
	uint64_t unblocked_king_w = attackr( king, blockers, ray_w );
	uint64_t unblocked_king_ne = attack( king, blockers, ray_ne );
	uint64_t unblocked_king_se = attackr( king, blockers, ray_se );
	uint64_t unblocked_king_sw = attackr( king, blockers, ray_sw );
	uint64_t unblocked_king_nw = attack( king, blockers, ray_nw );

	uint64_t rooks_and_queens = p.bitboards[1-c].b[bb_type::rooks] | p.bitboards[1-c].b[bb_type::queens];
	process_ray( p, c, *this, unblocked_king_n, rooks_and_queens, king );
	process_ray( p, c, *this, unblocked_king_e, rooks_and_queens, king );
	process_ray( p, c, *this, unblocked_king_s, rooks_and_queens, king );
	process_ray( p, c, *this, unblocked_king_w, rooks_and_queens, king );

	uint64_t bishops_and_queens = p.bitboards[1-c].b[bb_type::bishops] | p.bitboards[1-c].b[bb_type::queens];
	bishops_and_queens |= pawn_control[c][king] & p.bitboards[1-c].b[bb_type::pawns];
	process_ray( p, c, *this, unblocked_king_ne, bishops_and_queens, king );
	process_ray( p, c, *this, unblocked_king_se, bishops_and_queens, king );
	process_ray( p, c, *this, unblocked_king_sw, bishops_and_queens, king );
	process_ray( p, c, *this, unblocked_king_nw, bishops_and_queens, king );

	uint64_t knights = possible_knight_moves[king] & p.bitboards[1-c].b[bb_type::knights];
	while( knights ) {
		uint64_t knight = bitscan_unset( knights );
		calc_check_map_knight( *this, king, knight );
	}

	check = board[king];
}
