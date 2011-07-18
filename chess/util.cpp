#include "chess.hpp"
#include "assert.hpp"
#include "calc.hpp"
#include "detect_check.hpp"
#include "eval.hpp"
#include "util.hpp"
#include "random.hpp"
#include "platform.hpp"

#include <iostream>

bool validate_move( position const& p, move const& m, color::type c )
{
	check_map check;
	calc_check_map( p, c, check );

	move_info moves[200];
	move_info* pm = moves;
	int ev = evaluate( p, c );
	calculate_moves( p, c, ev, pm, check );

	for( move_info* it = moves; it != pm; ++it ) {
		if( it->m.piece == m.piece && it->m.target_col == m.target_col && it->m.target_row == m.target_row ) {
			return true;
		}
	}

	return false;
}

void parse_move( position& p, color::type& c, std::string const& line )
{
	if( line.size() < 4 || line.size() > 5 ) {
		std::cout << "Error (unknown command): " << line << std::endl;
		return;
	}

	if( line[0] < 'a' || line[0] > 'h') {
		std::cout << "Error (unknown command): " << line << std::endl;
		return;
	}
	if( line[1] < '1' || line[1] > '8') {
		std::cout << "Error (unknown command): " << line << std::endl;
		return;
	}
	if( line[2] < 'a' || line[2] > 'h') {
		std::cout << "Error (unknown command): " << line << std::endl;
		return;
	}
	if( line[3] < '1' || line[3] > '8') {
		std::cout << "Error (unknown command): " << line << std::endl;
		return;
	}

	if( line.size() == 5 && line[4] != 'q' ) {
		std::cout << "Illegal move (not promoting to queen is disallowed due to lazy developer): " << line << std::endl;
		return;
	}

	int col = line[0] - 'a';
	int row = line[1] - '1';

	unsigned char pi = p.board[col][row];
	if( pi == pieces::nil ) {
		std::cout << "Illegal move (no piece in source square): " << line << std::endl;
		return;
	}
	if( (pi >> 4) != c ) {
		std::cout << "Illegal move (piece in source square of wrong color): " << line << std::endl;
		return;
	}
	pi &= 0xf;

	move m;
	m.piece = pi;
	m.target_col = line[2] - 'a';
	m.target_row = line[3] - '1';

	if( !validate_move( p, m, c ) ) {
		std::cout << "Illegal move: " << line << std::endl;
		return;
	}

	apply_move( p, m, c );
	c = static_cast<color::type>( 1 - c );
}

std::string move_to_string( position const& p, color::type c, move const& m )
{
	std::string ret;

	if( m.piece == pieces::king ) {
		if( m.target_col == 6 && p.pieces[c][m.piece].column == 4 ) {
			return "   O-O  ";
		}
		else if( m.target_col == 2 && p.pieces[c][m.piece].column == 4 ) {
			return " O-O-O  ";
		}
	}

	switch( m.piece ) {
		case pieces::bishop1:
		case pieces::bishop2:
			ret += 'B';
			break;
		case pieces::knight1:
		case pieces::knight2:
			ret += 'N';
			break;
		case pieces::queen:
			ret += 'Q';
			break;
		case pieces::king:
			ret += 'K';
			break;
		case pieces::rook1:
		case pieces::rook2:
			ret += 'R';
			break;
		case pieces::pawn1:
		case pieces::pawn2:
		case pieces::pawn3:
		case pieces::pawn4:
		case pieces::pawn5:
		case pieces::pawn6:
		case pieces::pawn7:
		case pieces::pawn8:
			if( p.pieces[c][m.piece].special ) {
				switch( p.promotions[c] >> (2 * (m.piece - pieces::pawn1) ) & 0x03 ) {
					case promotions::queen:
						ret += 'Q';
						break;
					case promotions::rook:
						ret += 'R';
						break;
					case promotions::bishop:
						ret += 'B';
						break;
					case promotions::knight:
						ret += 'N';
						break;
				}
			}
			else {
				ret += ' ';
			}
			break;
		default:
			break;
	}

	ret += 'a' + p.pieces[c][m.piece].column;
	ret += '1' + p.pieces[c][m.piece].row;

	if( p.board[m.target_col][m.target_row] != pieces::nil ) {
		ret += 'x';
	}
	else if( m.piece >= pieces::pawn1 && m.piece <= pieces::pawn8
		&& p.pieces[c][m.piece].column != m.target_col && !p.pieces[c][m.piece].special )
	{
		// Must be en-passant
		ret += 'x';
	}
	else {
		ret += '-';
	}

	ret += 'a' + m.target_col;
	ret += '1' + m.target_row;

	if( m.piece >= pieces::pawn1 && m.piece <= pieces::pawn8 && !p.pieces[c][m.piece].special ) {
		if( m.target_row == 0 || m.target_row == 7 ) {
			ret += "=Q";
		}
		else {
			ret += "  ";
		}
	}
	else {
		ret += "  ";
	}

	return ret;
}

void init_board( position& p )
{
	for( unsigned int c = 0; c <= 1; ++c ) {
		for( unsigned int i = 0; i < 8; ++i ) {
			p.pieces[c][pieces::pawn1 + i].column = i;
			p.pieces[c][pieces::pawn1 + i].row = (c == color::black) ? 6 : 1;

			p.pieces[c][pieces::pawn8 + 1 + i].row = (c == color::black) ? 7 : 0;
		}

		p.pieces[c][pieces::rook1].column = 0;
		p.pieces[c][pieces::rook2].column = 7;
		p.pieces[c][pieces::bishop1].column = 2;
		p.pieces[c][pieces::bishop2].column = 5;
		p.pieces[c][pieces::knight1].column = 1;
		p.pieces[c][pieces::knight2].column = 6;
		p.pieces[c][pieces::king].column = 4;
		p.pieces[c][pieces::queen].column = 3;
	}

	for( unsigned int x = 0; x < 8; ++x ) {
		for( unsigned int y = 0; y < 8; ++y ) {
			p.board[x][y] = pieces::nil;
		}
	}

	for( unsigned int c = 0; c <= 1; ++c ) {
		for( unsigned int i = 0; i < 16; ++i ) {
			p.pieces[c][i].alive = true;
			p.pieces[c][i].special = false;

			p.board[p.pieces[c][i].column][p.pieces[c][i].row] = i | (c << 4);
		}

		p.pieces[c][pieces::rook1].special = true;
		p.pieces[c][pieces::rook2].special = true;
	}

	p.promotions[0] = 0;
	p.promotions[1] = 0;

	p.can_en_passant = pieces::nil;
}

void init_board_from_pieces( position& p )
{
	for( unsigned int x = 0; x < 8; ++x ) {
		for( unsigned int y = 0; y < 8; ++y ) {
			p.board[x][y] = pieces::nil;
		}
	}

	for( unsigned int c = 0; c <= 1; ++c ) {
		for( unsigned int i = 0; i < 16; ++i ) {
			if( p.pieces[c][i].alive ) {
				p.board[p.pieces[c][i].column][p.pieces[c][i].row] = i | (c << 4);
			}
		}
	}
}

bool apply_move( position& p, move const& m, color::type c )
{
	bool ret = false;

	piece& pp = p.pieces[c][m.piece];

	if( !pp.alive ) {
		std::cerr << "FAIL, moving dead piece!" << (int)m.piece << " " << (int)m.target_col << " " << (int)m.target_row << std::endl;
	}
	ASSERT( pp.alive );

	p.board[pp.column][pp.row] = pieces::nil;

	// Handle castling
	if( m.piece == pieces::king ) {
		if( pp.column == 4 ) {
			if( m.target_col == 6 ) {
				// Kingside
				ASSERT( !pp.special );
				ASSERT( p.pieces[c][pieces::rook2].special );
				ASSERT( p.pieces[c][pieces::rook2].alive );
				pp.special = 1;
				pp.column = 6;
				p.board[6][m.target_row] = pieces::king | (c << 4);
				p.board[7][m.target_row] = pieces::nil;
				p.board[5][m.target_row] = pieces::rook2 | (c << 4);
				p.pieces[c][pieces::rook2].column = 5;
				p.pieces[c][pieces::rook1].special = 0;
				p.pieces[c][pieces::rook2].special = 0;
				p.can_en_passant = pieces::nil;
				return false;
			}
			else if( m.target_col == 2 ) {
				// Queenside
				pp.special = 1;
				pp.column = 2;
				p.board[2][m.target_row] = pieces::king | (c << 4);
				p.board[0][m.target_row] = pieces::nil;
				p.board[3][m.target_row] = pieces::rook1 | (c << 4);
				p.pieces[c][pieces::rook1].column = 3;
				p.pieces[c][pieces::rook1].special = 0;
				p.pieces[c][pieces::rook2].special = 0;
				p.can_en_passant = pieces::nil;
				return false;
			}
		}
		p.pieces[c][pieces::rook1].special = 0;
		p.pieces[c][pieces::rook2].special = 0;
	}
	else if( m.piece == pieces::rook1 ) {
		pp.special = 0;
	}
	else if( m.piece == pieces::rook2 ) {
		pp.special = 0;
	}

	bool const is_pawn = m.piece >= pieces::pawn1 && m.piece <= pieces::pawn8 && !pp.special;

	unsigned char old_piece = p.board[m.target_col][m.target_row];
	if( old_piece != pieces::nil ) {
		// Ordinary capture
		ASSERT( (old_piece >> 4) != c );
		old_piece &= 0x0f;
		ASSERT( old_piece != pieces::king );
		p.pieces[1-c][old_piece].alive = false;
		p.pieces[1-c][old_piece].special = false;

		ret = true;
	}
	else if( is_pawn && p.pieces[c][m.piece].column != m.target_col )
	{
		// Must be en-passant
		old_piece = p.board[m.target_col][pp.row];
		ASSERT( (old_piece >> 4) != c );
		old_piece &= 0x0f;
		ASSERT( p.can_en_passant == old_piece );
		ASSERT( old_piece >= pieces::pawn1 && old_piece <= pieces::pawn8 );
		ASSERT( old_piece != pieces::king );
		p.pieces[1-c][old_piece].alive = false;
		p.pieces[1-c][old_piece].special = false;
		p.board[m.target_col][pp.row] = pieces::nil;

		ret = true;
	}

	if( is_pawn ) {
		if( m.target_row == 0 || m.target_row == 7) {
			pp.special = true;
			p.promotions[c] |= promotions::queen << (2 * (m.piece - pieces::pawn1) );
			p.can_en_passant = pieces::nil;
		}
		else if( (c == color::white) ? (pp.row + 2 == m.target_row) : (m.target_row + 2 == pp.row) ) {
			p.can_en_passant = m.piece;
		}
		else {
			p.can_en_passant = pieces::nil;
		}
	}
	else {
		p.can_en_passant = pieces::nil;
	}

	p.board[m.target_col][m.target_row] = m.piece | (c << 4);

	pp.column = m.target_col;
	pp.row = m.target_row;

	return ret;
}


namespace {
static mutex m;
static unsigned int random_unsigned_long_long_pos = 0;
static unsigned int random_unsigned_char = 0;
}

void init_random( int seed )
{
	random_unsigned_char = seed;
	random_unsigned_long_long_pos = (seed + 0xf00) & sizeof(precomputed_random_data);
}


unsigned char get_random_unsigned_char()
{
	return 0;
	scoped_lock l( m ) ;
	if( ++random_unsigned_char >= sizeof(precomputed_random_data) ) {
		random_unsigned_char = 0;
	}
	return precomputed_random_data[random_unsigned_char];
}

unsigned long long get_random_unsigned_long_long() {
	scoped_lock l( m ) ;
	random_unsigned_long_long_pos += sizeof( unsigned long long );

	if( random_unsigned_long_long_pos >= (sizeof(precomputed_random_data) - 8) ) {
		random_unsigned_long_long_pos = 0;
	}

	return *reinterpret_cast<unsigned long long*>(precomputed_random_data + random_unsigned_long_long_pos);
}
