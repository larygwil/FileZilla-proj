#include "zobrist.hpp"
#include "chess.hpp"
#include "util.hpp"

namespace {
static unsigned long long data[2][16][8][8];
unsigned long long enpassant[8];

unsigned long long promoted_pawns[2][8];
unsigned long long can_castle[2][2];
unsigned long long castled[2];

unsigned long long pawn_structure[2][8][8];

bool initialized = false;
}

void init_zobrist_table( unsigned int pi )
{
	for( unsigned int c = 0; c < 2; ++c ) {
		for( unsigned int col = 0; col < 8; ++col ) {
			for( unsigned int row = 0; row < 8; ++row ) {
				data[c][pi][col][row] = get_random_unsigned_long_long();
			}
		}
	}
}

void zobrist_copy( unsigned int source, unsigned int target )
{
	for( unsigned int c = 0; c < 2; ++c ) {
		for( unsigned int col = 0; col < 8; ++col ) {
			for( unsigned int row = 0; row < 8; ++row ) {
				data[c][target][col][row] = data[c][source][col][row];
			}
		}
	}
}

void init_zobrist_tables()
{
	if( initialized ) {
		return;
	}

	init_zobrist_table( pieces::pawn1 );
	init_zobrist_table( pieces::knight1 );
	init_zobrist_table( pieces::bishop1 );
	init_zobrist_table( pieces::rook1 );
	init_zobrist_table( pieces::queen );
	init_zobrist_table( pieces::king );
	for( unsigned int i = pieces::pawn2; i <= pieces::pawn8; ++i ) {
		//zobrist_copy( pieces::pawn1, i );
		// Sadly cannot merge pawns as it screws with promotions right now
		init_zobrist_table( i );
	}
	zobrist_copy( pieces::knight1, pieces::knight2 );
	zobrist_copy( pieces::bishop1, pieces::bishop2 );
	zobrist_copy( pieces::rook1, pieces::rook2 );

	for( unsigned int c = 0; c < 2; ++c ) {
		for( unsigned int pi = 0; pi < 8; ++pi ) {
			promoted_pawns[c][pi] = get_random_unsigned_long_long();
		}
		can_castle[c][0] = get_random_unsigned_long_long();
		can_castle[c][1] = get_random_unsigned_long_long();
		castled[c] = get_random_unsigned_long_long();
	}

	for( unsigned int i = 0; i < 8; ++i ) {
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
		for( unsigned int pi = 0; pi < 8; ++pi ) {
			piece const& pp = p.pieces[c][pi];
			if( pp.alive ) {
				ret ^= data[c][pi][pp.column][pp.row];
				if( pp.special ) {
					ret ^= promoted_pawns[c][pi];
				}
			}
		}
		for( unsigned int pi = 8; pi < 16; ++pi ) {
			piece const& pp = p.pieces[c][pi];
			if( pp.alive ) {
				ret ^= data[c][pi][pp.column][pp.row];
			}
		}

		{
			piece const& pp = p.pieces[c][pieces::rook1];
			if( pp.alive && pp.special ) {
				ret ^= can_castle[c][0];
			}
		}
		{
			piece const& pp = p.pieces[c][pieces::rook2];
			if( pp.alive && pp.special ) {
				ret ^= can_castle[c][1];
			}
		}

		{
			piece const& pp = p.pieces[c][pieces::king];
			if( pp.special ) {
				ret ^= castled[c];
			}
		}
	}

	if( p.can_en_passant != pieces::nil ) {
		ret ^= enpassant[p.can_en_passant];
	}

	return ret;
}

namespace {
static void subtract_target( position const& p, color::type c, unsigned long long& hash, int target, int col, int row )
{
	if( target >= pieces::pawn1 && target <= pieces::pawn8 ) {
		piece const& pp = p.pieces[1-c][target];
		if( pp.special ) {
			hash ^= promoted_pawns[1-c][target];
		}
	}
	else if( target == pieces::rook1 || target == pieces::rook2 ) {
		piece const& pp = p.pieces[1-c][target];
		if( pp.special ) {
			hash ^= can_castle[1-c][target - pieces::rook1];
		}
	}
	hash ^= data[1-c][target][col][row];
}
}

unsigned long long update_zobrist_hash( position const& p, color::type c, unsigned long long hash, move const& m )
{
	if( p.can_en_passant != pieces::nil ) {
		hash ^= enpassant[p.can_en_passant];
	}

	int target = p.board[m.target_col][m.target_row];
	if( target != pieces::nil ) {
		target &= 0x0f;
		subtract_target( p, c, hash, target, m.target_col, m.target_row );
	}

	unsigned char source = p.board[m.source_col][m.source_row] & 0x0f;

	piece const& pp = p.pieces[c][source];
	if( source >= pieces::pawn1 && source <= pieces::pawn8 && !pp.special ) {
		if( m.target_col != pp.column && target == pieces::nil ) {
			// Was en-passant
			hash ^= data[1-c][p.board[m.target_col][pp.row] & 0x0f][m.target_col][pp.row];
		}
		else if( (m.target_row == 0 || m.target_row == 7) ) {
			// Promotion
			hash ^= promoted_pawns[c][source];
		}
		else if( m.target_row == pp.row + 2 || m.target_row + 2 == pp.row ) {
			// Becomes en-passantable
			hash ^= enpassant[source];
		}
	}
	else if( source == pieces::rook1 || source == pieces::rook2 ) {
		if( pp.special ) {
			hash ^= can_castle[c][source - pieces::rook1];
		}
	}
	else if( source == pieces::king ) {
		if( (pp.column == m.target_col + 2) || (pp.column + 2 == m.target_col) ) {
			// Was castling
			if( m.target_col == 2 ) {
				hash ^= data[c][pieces::rook1][0][m.target_row];
				hash ^= data[c][pieces::rook1][3][m.target_row];
			}
			else {
				hash ^= data[c][pieces::rook2][7][m.target_row];
				hash ^= data[c][pieces::rook2][5][m.target_row];
			}
			hash ^= castled[c];
		}
		piece const& r1 = p.pieces[c][pieces::rook1];
		if( r1.alive && r1.special ) {
			hash ^= can_castle[c][0];
		}
		piece const& r2 = p.pieces[c][pieces::rook2];
		if( r2.alive && r2.special ) {
			hash ^= can_castle[c][1];
		}
	}
	hash ^= data[c][source][pp.column][pp.row];
	hash ^= data[c][source][m.target_col][m.target_row];

	return hash;
}

unsigned long long get_pawn_structure_hash( color::type c, unsigned char col, unsigned char row )
{
	return pawn_structure[c][col][row];
}
