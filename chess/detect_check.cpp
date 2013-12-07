#include "chess.hpp"
#include "detect_check.hpp"
#include "magic.hpp"
#include "tables.hpp"

bool detect_check_knights( position const& p, color::type c, uint64_t king )
{
	uint64_t knights = possible_knight_moves[ king ];
	knights &= p.bitboards[other(c)][bb_type::knights];

	return knights != 0;
}

bool detect_check( position const& p, color::type c, uint64_t king,uint64_t ignore )
{
	uint64_t blockers = p.bitboards[other(c)][bb_type::all_pieces] | p.bitboards[c][bb_type::all_pieces];
	blockers &= ~(1ull << ignore);

	uint64_t rooks_and_queens = p.bitboards[other(c)][bb_type::rooks] | p.bitboards[other(c)][bb_type::queens];
	uint64_t bishops_and_queens = p.bitboards[other(c)][bb_type::bishops] | p.bitboards[other(c)][bb_type::queens];
	bishops_and_queens |= pawn_control[c][king] & p.bitboards[other(c)][bb_type::pawns];

	uint64_t rooks_and_queens_check = rook_magic( king, blockers ) & rooks_and_queens;
	uint64_t bishops_and_queens_check = bishop_magic( king, blockers ) & bishops_and_queens;

	return rooks_and_queens_check || bishops_and_queens_check || detect_check_knights( p, c, king );
}

bool detect_check( position const& p, color::type c )
{
	return detect_check( p, c, p.king_pos[c], p.king_pos[c] );
}


void check_map::process_direct_check( position const& p, uint64_t piece )
{
	unsigned char cpi = static_cast<unsigned char>(piece) | 0x80;
	board[piece] = cpi;

	if( !board[p.king_pos[p.self()]] ) {
		board[p.king_pos[p.self()]] = cpi;
	}
	else {
		board[p.king_pos[p.self()]] = 0x80 | 0x40;
	}
}


void check_map::process_slider( position const& p, uint64_t piece )
{
	uint64_t between = between_squares[piece][p.king_pos[p.self()]];

	uint64_t block_count = popcount( between & p.bitboards[p.self()][bb_type::all_pieces] );
	if( block_count < 2 ) {
		unsigned char cpi = static_cast<unsigned char>(piece) | 0x80;
		board[piece] = cpi;

		while( between ) {
			uint64_t sq = bitscan_unset( between );
			board[sq] = cpi;
		}

		if( !block_count ) {
			if( !board[p.king_pos[p.self()]] ) {
				board[p.king_pos[p.self()]] = cpi;
			}
			else {
				board[p.king_pos[p.self()]] = 0x80 | 0x40;
			}
		}
	}
}


check_map::check_map( position const& p )
	: board()
{
	uint64_t slider_checks = rook_magic( p.king_pos[p.self()], p.bitboards[p.other()][bb_type::all_pieces] ) & (p.bitboards[p.other()][bb_type::rooks] | p.bitboards[p.other()][bb_type::queens]);
	slider_checks |= bishop_magic( p.king_pos[p.self()], p.bitboards[p.other()][bb_type::all_pieces] ) & (p.bitboards[p.other()][bb_type::bishops] | p.bitboards[p.other()][bb_type::queens]);
	while( slider_checks ) {
		uint64_t piece = bitscan_unset( slider_checks );
		process_slider( p, piece );
	}

	uint64_t direct_checks = possible_knight_moves[p.king_pos[p.self()]] & p.bitboards[p.other()][bb_type::knights];
	direct_checks |= pawn_control[p.self()][p.king_pos[p.self()]] & p.bitboards[p.other()][bb_type::pawns];
	while( direct_checks ) {
		uint64_t piece = bitscan_unset( direct_checks );
		process_direct_check( p, piece );
	}

	check = board[p.king_pos[p.self()]];
}
