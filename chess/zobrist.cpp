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

unsigned long long enpassant[128];

unsigned long long castle[2][5];

unsigned long long pawn_structure[2][8][8];

bool initialized = false;
}

void init_zobrist_tables()
{
	if( initialized ) {
		return;
	}

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

	for( unsigned int i = 0; i < 128; ++i ) {
		enpassant[i] = get_random_unsigned_long_long();
	}

	for( unsigned int c = 0; c < 2; ++c ) {
		for( unsigned int col = 0; col < 8; ++col ) {
			for( unsigned int row = 0; row < 8; ++row ) {
				pawn_structure[c][col][row] = get_random_unsigned_long_long();
			}
		}
	}

	initialized = true;
}

unsigned long long get_zobrist_hash( position const& p, color::type c ) {
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

	if( p.can_en_passant ) {
		ret ^= enpassant[p.can_en_passant];
	}

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
	if( p.can_en_passant ) {
		hash ^= enpassant[p.can_en_passant];
	}

	int target = p.board[m.target_col][m.target_row];
	if( target ) {
		target &= 0x0f;
		hash ^= get_piece_hash( static_cast<pieces::type>(target), static_cast<color::type>(1-c), m.target_col + m.target_row * 8 );
		
		if( target == pieces::rook ) {
			if( !m.target_col && p.castle[1-c] & 0x2 && m.target_row == (c ? 0 : 7) ) {
				hash ^= castle[1-c][p.castle[1-c]];
				hash ^= castle[1-c][p.castle[1-c] & 0x5];
			}
			else if( m.target_col == 7 && p.castle[1-c] & 0x1 && m.target_row == (c ? 0 : 7) ) {
				hash ^= castle[1-c][p.castle[1-c]];
				hash ^= castle[1-c][p.castle[1-c] & 0x6];
			}
		}
	}

	pieces::type source = static_cast<pieces::type>(p.board[m.source_col][m.source_row] & 0x0f);
	hash ^= get_piece_hash( source, c, m.source_col + m.source_row * 8 );

	if( source == pieces::pawn ) {
		if( m.target_col != m.source_col && !target ) {
			// Was en-passant
			hash ^= pawns[1-c][m.target_col + m.source_row * 8];
		}
		if( m.target_row == m.source_row + 2 || m.target_row + 2 == m.source_row ) {
			// Becomes en-passantable
			hash ^= enpassant[m.target_col + m.target_row * 8 + (c ? 64 : 0)];
		}
	}
	else if( source == pieces::rook ) {
		if( !m.source_col && p.castle[c] & 0x2 && m.source_row == (c ? 7 : 0) ) {
			hash ^= castle[c][p.castle[c]];
			hash ^= castle[c][p.castle[c] & 0x5];
		}
		else if( m.source_col == 7 && p.castle[c] & 0x1 && m.source_row == (c ? 7 : 0) ) {
			hash ^= castle[c][p.castle[c]];
			hash ^= castle[c][p.castle[c] & 0x6];
		}
	}
	else if( source == pieces::king ) {
		if( (m.source_col == m.target_col + 2) || (m.source_col + 2 == m.target_col) ) {
			// Was castling
			if( m.target_col == 2 ) {
				hash ^= rooks[c][0 + m.target_row * 8];
				hash ^= rooks[c][3 + m.target_row * 8];
			}
			else {
				hash ^= rooks[c][7 + m.target_row * 8];
				hash ^= rooks[c][5 + m.target_row * 8];
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
		hash ^= get_piece_hash( source, c, m.target_col + m.target_row * 8 );
	}
	else {
		hash ^= queens[c][m.target_col + m.target_row * 8];
	}

	return hash;
}

unsigned long long get_pawn_structure_hash( color::type c, unsigned char col, unsigned char row )
{
	return pawn_structure[c][col][row];
}
