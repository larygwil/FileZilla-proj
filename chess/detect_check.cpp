#include "chess.hpp"
#include "detect_check.hpp"
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
	return detect_check( p, c, p.king_pos[c], p.king_pos[c] );
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


static void process_piece( position const& p, check_map& map, uint64_t piece )
{
	uint64_t between = between_squares[piece][p.king_pos[p.self()]];

	uint64_t block_count = popcount( between & p.bitboards[p.self()].b[bb_type::all_pieces] );
	if( block_count < 2 ) {
		uint64_t cpi = piece | 0x80;

		map.board[piece] = cpi;

		while( between ) {
			uint64_t sq = bitscan_unset( between );
			map.board[sq] = cpi;
		}

		if( !block_count ) {
			if( !map.board[p.king_pos[p.self()]] ) {
				map.board[p.king_pos[p.self()]] = cpi;
			}
			else {
				map.board[p.king_pos[p.self()]] = 0x80 | 0x40;
			}
		}
	}
}


check_map::check_map( position const& p )
{
	memset( board, 0, sizeof(board) );

	uint64_t potential_rook_checks = rook_magic( p.king_pos[p.self()], p.bitboards[p.other()].b[bb_type::all_pieces] ) & (p.bitboards[p.other()].b[bb_type::rooks] | p.bitboards[p.other()].b[bb_type::queens]);
	while( potential_rook_checks ) {
		uint64_t rook = bitscan_unset( potential_rook_checks );
		process_piece( p, *this, rook );
	}
	uint64_t potential_bishop_checks = bishop_magic( p.king_pos[p.self()], p.bitboards[p.other()].b[bb_type::all_pieces] ) & (p.bitboards[p.other()].b[bb_type::bishops] | p.bitboards[p.other()].b[bb_type::queens]);
	potential_bishop_checks |= pawn_control[p.self()][p.king_pos[p.self()]] & p.bitboards[p.other()].b[bb_type::pawns];
	while( potential_bishop_checks ) {
		uint64_t bishop = bitscan_unset( potential_bishop_checks );
		process_piece( p, *this, bishop );
	}

	uint64_t knights = possible_knight_moves[p.king_pos[p.self()]] & p.bitboards[p.other()].b[bb_type::knights];
	while( knights ) {
		uint64_t knight = bitscan_unset( knights );
		calc_check_map_knight( *this, p.king_pos[p.self()], knight );
	}

	check = board[p.king_pos[p.self()]];
}
