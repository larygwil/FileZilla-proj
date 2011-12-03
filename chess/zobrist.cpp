#include "zobrist.hpp"
#include "chess.hpp"
#include "util.hpp"

namespace {
static unsigned long long pawns[2][64];
static unsigned long long knights[2][64];
static unsigned long long bishops[2][64];
static unsigned long long rooks[2][64];
static unsigned long long queens[2][64];
static unsigned long long kings[2][64];

unsigned long long enpassant[64];

unsigned long long castle[2][5];

unsigned long long pawn_structure[2][64];

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


unsigned long long get_zobrist_hash( position const& p ) {
	unsigned long long ret = 0;

	for( unsigned int c = 0; c < 2; ++c ) {
		unsigned long long pieces = p.bitboards[c].b[bb_type::all_pieces];
		while( pieces ) {
			unsigned long long piece;
			bitscan( pieces, piece );

			pieces &= pieces - 1;

			unsigned long long bpiece = 1ull << piece;
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

	return ret;
}

namespace {
static unsigned long long get_piece_hash( pieces::type pi, color::type c, int pos )
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
		default:
		case pieces::king:
			return kings[c][pos];
	}
}
}

unsigned long long update_zobrist_hash( position const& p, color::type c, unsigned long long hash, move const& m )
{
	hash ^= enpassant[p.can_en_passant];

	int captured = p.board[m.target];
	if( captured ) {
		captured &= 0x0f;
		hash ^= get_piece_hash( static_cast<pieces::type>(captured), static_cast<color::type>(1-c), m.target );
		
		if( captured == pieces::rook ) {
			if( m.target == queenside_rook_origin[1-c] && p.castle[1-c] & 0x2 ) {
				hash ^= castle[1-c][p.castle[1-c]];
				hash ^= castle[1-c][p.castle[1-c] & 0x5];
			}
			else if( m.target == kingside_rook_origin[1-c] && p.castle[1-c] & 0x1 ) {
				hash ^= castle[1-c][p.castle[1-c]];
				hash ^= castle[1-c][p.castle[1-c] & 0x6];
			}
		}
	}

	pieces::type source = static_cast<pieces::type>(p.board[m.source] & 0x0f);
	hash ^= get_piece_hash( source, c, m.source );

	if( source == pieces::pawn ) {
		unsigned char source_row = m.source / 8;
		unsigned char target_col = m.target % 8;
		unsigned char target_row = m.target / 8;
		if( m.flags & move_flags::enpassant ) {
			// Was en-passant
			hash ^= pawns[1-c][target_col + source_row * 8];
		}
		else if( m.flags & move_flags::pawn_double_move ) {
			// Becomes en-passantable
			hash ^= enpassant[target_col + (source_row + target_row) * 4];
		}
	}
	else if( source == pieces::rook ) {
		if( m.source == queenside_rook_origin[c] && p.castle[c] & 0x2 ) {
			hash ^= castle[c][p.castle[c]];
			hash ^= castle[c][p.castle[c] & 0x5];
		}
		else if( m.source == kingside_rook_origin[c] && p.castle[c] & 0x1 ) {
			hash ^= castle[c][p.castle[c]];
			hash ^= castle[c][p.castle[c] & 0x6];
		}
	}
	else if( source == pieces::king ) {
		if( m.flags & move_flags::castle ) {
			unsigned char target_col = m.target % 8;
			unsigned char target_row = m.target / 8;

			// Was castling
			if( target_col == 2 ) {
				hash ^= rooks[c][0 + target_row * 8];
				hash ^= rooks[c][3 + target_row * 8];
			}
			else {
				hash ^= rooks[c][7 + target_row * 8];
				hash ^= rooks[c][5 + target_row * 8];
			}
			hash ^= castle[c][p.castle[c]];
			hash ^= castle[c][0x4];
		}
		else {
			hash ^= castle[c][p.castle[c]];
			hash ^= castle[c][p.castle[c] & 0x4];
		}
	}

	if( !(m.flags & move_flags::promotion ) ) {
		hash ^= get_piece_hash( source, c, m.target );
	}
	else {
		hash ^= queens[c][m.target];
	}

	return hash;
}

unsigned long long get_pawn_structure_hash( color::type c, unsigned char pawn )
{
	return pawn_structure[c][pawn];
}
