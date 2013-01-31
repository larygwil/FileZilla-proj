#include "zobrist.hpp"
#include "chess.hpp"
#include "util.hpp"
#include "random.hpp"

namespace {
static uint64_t pawns[2][64];
static uint64_t knights[2][64];
static uint64_t bishops[2][64];
static uint64_t rooks[2][64];
static uint64_t queens[2][64];
static uint64_t kings[2][64];

uint64_t enpassant[64];

uint64_t castle[2][5];

uint64_t pawn_structure[2][64];

bool initialized = false;
}

extern unsigned char const queenside_rook_origin[2] = {
	0, 56
};
extern unsigned char const kingside_rook_origin[2] = {
	7, 63
};


void init_zobrist_tables()
{
	if( initialized ) {
		return;
	}

	push_rng_state();
	init_random( 0 );

	for( int c = 0; c < 2; ++c ) {
		for( int i = 0; i < 64; ++i ) {
			pawns[c][i] = get_random_unsigned_long_long();
			knights[c][i] = get_random_unsigned_long_long();
			bishops[c][i] = get_random_unsigned_long_long();
			rooks[c][i] = get_random_unsigned_long_long();
			queens[c][i] = get_random_unsigned_long_long();
			kings[c][i] = get_random_unsigned_long_long();
		}
	}
	
	for( unsigned int c = 0; c < 2; ++c ) {
		for( int i = 0; i < 5; ++i ) {
			castle[c][i] = get_random_unsigned_long_long();
		}
	}

	enpassant[0] = 0;
	for( unsigned int i = 1; i < 64; ++i ) {
		if( i / 8 == 2 || i / 8 == 5 ) {
			enpassant[i] = get_random_unsigned_long_long();
		}
		else {
			enpassant[i] = 0;
		}
	}

	for( unsigned int c = 0; c < 2; ++c ) {
		for( unsigned int pawn = 0; pawn < 64; ++pawn ) {
			pawn_structure[c][pawn] = get_random_unsigned_long_long();
		}
	}

	initialized = true;

	pop_rng_state();
}


uint64_t get_zobrist_hash( position const& p )
{
	uint64_t ret = 0;

	for( unsigned int c = 0; c < 2; ++c ) {
		uint64_t pieces = p.bitboards[c].b[bb_type::all_pieces];
		while( pieces ) {
			uint64_t piece = bitscan_unset( pieces );

			uint64_t bpiece = 1ull << piece;
			if( p.bitboards[c].b[bb_type::pawns] & bpiece ) {
				ret ^= pawns[c][piece];
			}
			else if( p.bitboards[c].b[bb_type::knights] & bpiece ) {
				ret ^= knights[c][piece];
			}
			else if( p.bitboards[c].b[bb_type::bishops] & bpiece ) {
				ret ^= bishops[c][piece];
			}
			else if( p.bitboards[c].b[bb_type::rooks] & bpiece ) {
				ret ^= rooks[c][piece];
			}
			else if( p.bitboards[c].b[bb_type::queens] & bpiece ) {
				ret ^= queens[c][piece];
			}
			else {//if( p.bitboards[c].b[bb_type::king] & bpiece ) {
				ret ^= kings[c][piece];
			}
		}

		ret ^= castle[c][p.castle[c]];
	}

	ret ^= enpassant[p.can_en_passant];

	if( !p.white() ) {
		ret = ~ret;
	}

	return ret;
}


namespace {
static uint64_t get_piece_hash( pieces::type pi, color::type c, int pos )
{
	switch( pi ) {
		case pieces::pawn:
			return pawns[c][pos];
		case pieces::knight:
			return knights[c][pos];
		case pieces::bishop:
			return bishops[c][pos];
		case pieces::rook:
			return rooks[c][pos];
		case pieces::queen:
			return queens[c][pos];
		case pieces::king:
			return kings[c][pos];
		default:
			return 0;
	}
}
}

uint64_t update_zobrist_hash( position const& p, uint64_t hash, move const& m )
{
	hash = ~hash;
	hash ^= enpassant[p.can_en_passant];

	pieces::type piece = p.get_piece( m );
	pieces::type captured_piece = p.get_captured_piece( m );

	if( m.enpassant() ) {
		// Was en-passant
		hash ^= pawns[p.other()][(m.target() % 8) | (m.source() & 0xf8)];
	}
	else if( captured_piece != pieces::none ) {
		hash ^= get_piece_hash( static_cast<pieces::type>(captured_piece), p.other(), m.target() );
		
		if( captured_piece == pieces::rook ) {
			if( m.target() == queenside_rook_origin[p.other()] && p.castle[p.other()] & 0x2 ) {
				hash ^= castle[p.other()][p.castle[p.other()]];
				hash ^= castle[p.other()][p.castle[p.other()] & 0x5];
			}
			else if( m.target() == kingside_rook_origin[p.other()] && p.castle[p.other()] & 0x1 ) {
				hash ^= castle[p.other()][p.castle[p.other()]];
				hash ^= castle[p.other()][p.castle[p.other()] & 0x6];
			}
		}
	}

	hash ^= get_piece_hash( piece, p.self(), m.source() );

	if( piece == pieces::pawn ) {
		if( (m.source() ^ m.target()) == 16 ) {
			unsigned char source_row = m.source() / 8;
			unsigned char target_col = m.target() % 8;
			unsigned char target_row = m.target() / 8;

			// Becomes en-passantable
			hash ^= enpassant[target_col + (source_row + target_row) * 4];
		}
	}
	else if( piece == pieces::rook ) {
		if( m.source() == queenside_rook_origin[p.self()] && p.castle[p.self()] & 0x2 ) {
			hash ^= castle[p.self()][p.castle[p.self()]];
			hash ^= castle[p.self()][p.castle[p.self()] & 0x5];
		}
		else if( m.source() == kingside_rook_origin[p.self()] && p.castle[p.self()] & 0x1 ) {
			hash ^= castle[p.self()][p.castle[p.self()]];
			hash ^= castle[p.self()][p.castle[p.self()] & 0x6];
		}
	}
	else if( piece == pieces::king ) {
		if( m.castle() ) {
			unsigned char target_col = m.target() % 8;
			unsigned char target_row = m.target() / 8;

			// Was castling
			if( target_col == 2 ) {
				hash ^= rooks[p.self()][0 + target_row * 8];
				hash ^= rooks[p.self()][3 + target_row * 8];
			}
			else {
				hash ^= rooks[p.self()][7 + target_row * 8];
				hash ^= rooks[p.self()][5 + target_row * 8];
			}
			hash ^= castle[p.self()][p.castle[p.self()]];
			hash ^= castle[p.self()][0x4];
		}
		else {
			hash ^= castle[p.self()][p.castle[p.self()]];
			hash ^= castle[p.self()][p.castle[p.self()] & 0x4];
		}
	}

	if( !m.promotion() ) {
		hash ^= get_piece_hash( piece, p.self(), m.target() );
	}
	else {
		pieces::type promotion = m.promotion_piece();
		switch( promotion ) {
			case pieces::knight:
				hash ^= knights[p.self()][m.target()];
				break;
			case pieces::bishop:
				hash ^= bishops[p.self()][m.target()];
				break;
			case pieces::rook:
				hash ^= rooks[p.self()][m.target()];
				break;
			case pieces::queen:
			default:
				hash ^= queens[p.self()][m.target()];
				break;
		}
	}

	return hash;
}

uint64_t get_pawn_structure_hash( color::type c, unsigned char pawn )
{
	return pawn_structure[c][pawn];
}


uint64_t get_enpassant_hash( unsigned char ep )
{
	return enpassant[ep];
}
