#include "chess.hpp"
#include "assert.hpp"
#include "calc.hpp"
#include "detect_check.hpp"
#include "eval.hpp"
#include "moves.hpp"
#include "util.hpp"
#include "random.hpp"
#include "pawn_structure_hash_table.hpp"
#include "platform.hpp"
#include "zobrist.hpp"

#include <iostream>

bool validate_move( position const& p, move const& m, color::type c )
{
	check_map check;
	calc_check_map( p, c, check );

	move_info moves[200];
	move_info* pm = moves;
	int ev = evaluate_fast( p, c );
	calculate_moves( p, c, ev, pm, check, killer_moves() );

	for( move_info* it = moves; it != pm; ++it ) {
		if( it->m == m ) {
			return true;
		}
	}

	return false;
}

bool parse_move( position& p, color::type c, std::string const& line, move& m )
{
	std::string str = line;
	std::size_t len = str.size();
	if( len && str[len - 1] == '#' ) {
		--len;
		str = str.substr(0, len);
	}
	else if( len && str[len - 1] == '+' ) {
		--len;
		str = str.substr(0, len);
	}

	bool promotion = false;
	if( len && (str[len - 1] == 'q' || str[len - 1] == 'r' || str[len - 1] == 'b' || str[len - 1] == 'n') ) {
		promotion = true;
		--len;
		str = str.substr(0, len);
	}

	if( str == "0-0" || str == "O-O" ) {
		m.source_col = 4;
		m.target_col = 6;
		m.source_row = c ? 7 : 0;
		m.target_row = m.source_row;
		if( !validate_move( p, m, c ) ) {
			std::cout << "Illegal move (not valid): " << line << std::endl;
			return false;
		}
		return true;
	}
	else if( str == "0-0-0" || str == "O-O-O" ) {
		m.source_col = 4;
		m.target_col = 2;
		m.source_row = c ? 7 : 0;
		m.target_row = m.source_row;
		if( !validate_move( p, m, c ) ) {
			std::cout << "Illegal move (not valid): " << line << std::endl;
			return false;
		}
		return true;
	}
	unsigned char piecetype = 0;

	if( len && str[len - 1] == 'Q' ) {
		--len;
		str = str.substr(0, len);
		if( str[len - 1] == '=' ) {
			--len;
			str = str.substr(0, len);
		}
		piecetype = 'P';
	}

	const char* s = str.c_str();

	switch( *s ) {
	case 'B':
	case 'Q':
	case 'K':
	case 'R':
	case 'N':
	case 'P':
		if( piecetype ) {
			std::cout << "Error (unknown command): " << line << std::endl;
			return false;
		}
		piecetype = *(s++);
		break;
	}

	int first_col = -1;
	int first_row = -1;
	int second_col = -1;
	int second_row = -1;

	bool got_separator = false;
	bool capture = false; // Not a capture or unknown

	while( *s ) {
		if( *s == 'x' || *s == ':' || *s == '-' ) {
			if( got_separator ) {
				std::cout << "Error (unknown command): " << line << std::endl;
				return false;
			}
			got_separator = true;
			if( *s == 'x' ) {
				capture = true;
			}
		}
		else if( *s >= 'a' && *s <= 'h' ) {
			if( second_col != -1 ) {
				std::cout << "Error (unknown command): " << line << std::endl;
				return false;
			}
			if( !got_separator && first_row == -1 && first_col == -1 ) {
				first_col = *s - 'a';
			}
			else {
				second_col = *s - 'a';
			}
		}
		else if( *s >= '1' && *s <= '8' ) {
			if( second_row != -1 ) {
				std::cout << "Error (unknown command): " << line << std::endl;
				return false;
			}
			if( !got_separator && second_col == -1 && first_row == -1 ) {
				first_row = *s - '1';
			}
			else {
				second_row = *s - '1';
			}
		}
		else {
			std::cout << "Error (unknown command): " << line << std::endl;
			return false;
		}

		++s;
	}

	if( !piecetype && (first_col == -1 || second_col == -1 || first_row == -1 || second_col == -1) ) {
		piecetype = 'P';
	}

	if( !got_separator && second_col == -1 && second_row == -1 ) {
		second_col = first_col;
		first_col = -1;
		second_row = first_row;
		first_row = -1;
	}

	if( first_col == -1 && first_row == -1 && second_col == -1 && second_row == -1 ) {
		std::cout << "Error (unknown command): " << line << std::endl;
		return false;
	}

	check_map check;
	calc_check_map( p, c, check );

	move_info moves[200];
	move_info* pm = moves;
	int ev = evaluate_fast( p, c );
	calculate_moves( p, c, ev, pm, check, killer_moves() );

	move_info* match = 0;

	for( move_info* it = moves; it != pm; ++it ) {
		unsigned char source = p.board2[it->m.source_col][it->m.source_row] & 0xf;

		char source_piece = 0;

		switch( source ) {
		case pieces2::king:
			source_piece = 'K';
			break;
		case pieces2::queen:
			source_piece = 'Q';
			break;
		case pieces2::rook:
			source_piece = 'R';
			break;
		case pieces2::bishop:
			source_piece = 'B';
			break;
		case pieces2::knight:
			source_piece = 'N';
			break;
		case pieces2::pawn:
			source_piece = 'P';
			break;
		}

		if( piecetype && piecetype != source_piece ) {
			continue;
		}

		if( capture && !it->m.captured_piece ) {
			continue;
		}

		if( first_col != -1 && first_col != it->m.source_col ) {
			continue;
		}
		if( first_row != -1 && first_row != it->m.source_row ) {
			continue;
		}
		if( second_col != -1 && second_col != it->m.target_col ) {
			continue;
		}
		if( second_row != -1 && second_row != it->m.target_row ) {
			continue;
		}

		if( match ) {
			std::cout << "Illegal move (ambigious): " << line << std::endl;
			return false;
		}
		else {
			match = it;
		}
	}


	if( !match ) {
		std::cout << "Illegal move (not valid): " << line << std::endl;
		std::cerr << "Parsed: " << first_col << " " << first_row << " " << second_col << " " << second_row << ", capture=" << capture << std::endl;
		return false;
	}

	if( promotion && match->m.target_row != (c ? 0 : 7) ) {
		std::cout << "Illegal move (not valid, expecting a promotion): " << line << std::endl;
	}

	m = match->m;

	return true;
}

std::string move_to_string( position const& p, color::type c, move const& m )
{
	std::string ret;

	unsigned char source = p.board2[m.source_col][m.source_row];

	if( (source >> 4 ) != c ) {
		std::cerr << "FAIL! Invalid move: "
				  << c << " "
				  << (source >> 4) << " "
				  << (source & 0xf) << " "
				  << static_cast<int>(m.source_col) << " "
				  << static_cast<int>(m.source_row) << " "
				  << static_cast<int>(m.target_col) << " "
				  << static_cast<int>(m.target_row) << " "
				  << std::endl;
	}

	source &= 0x0f;
	if( source == pieces2::king ) {
		if( m.target_col == 6 && m.source_col == 4 ) {
			return "   O-O  ";
		}
		else if( m.target_col == 2 && m.source_col == 4 ) {
			return " O-O-O  ";
		}
	}

	switch( source ) {
		case pieces2::bishop:
			ret += 'B';
			break;
		case pieces2::knight:
			ret += 'N';
			break;
		case pieces2::queen:
			ret += 'Q';
			break;
		case pieces2::king:
			ret += 'K';
			break;
		case pieces2::rook:
			ret += 'R';
			break;
		case pieces2::pawn:
			ret += ' ';
			break;
		default:
			break;
	}

	ret += 'a' + m.source_col;
	ret += '1' + m.source_row;

	if( m.captured_piece ) {
		ret += 'x';
	}
	else {
		ret += '-';
	}

	ret += 'a' + m.target_col;
	ret += '1' + m.target_row;

	if( m.flags & move_flags::promotion ) {
		ret += "=Q";
	}
	else {
		ret += "  ";
	}

	return ret;
}

void init_bitboards( position& p );

void init_board( position& p )
{
	memset( p.board2, 0, 64 );

	p.board2[0][0] = pieces2::rook;
	p.board2[1][0] = pieces2::knight;
	p.board2[2][0] = pieces2::bishop;
	p.board2[3][0] = pieces2::queen;
	p.board2[4][0] = pieces2::king;
	p.board2[5][0] = pieces2::bishop;
	p.board2[6][0] = pieces2::knight;
	p.board2[7][0] = pieces2::rook;

	p.board2[0][7] = pieces2::rook | (1 << 4);
	p.board2[1][7] = pieces2::knight | (1 << 4);
	p.board2[2][7] = pieces2::bishop | (1 << 4);
	p.board2[3][7] = pieces2::queen | (1 << 4);
	p.board2[4][7] = pieces2::king | (1 << 4);
	p.board2[5][7] = pieces2::bishop | (1 << 4);
	p.board2[6][7] = pieces2::knight | (1 << 4);
	p.board2[7][7] = pieces2::rook | (1 << 4);

	for( int i = 0; i < 8; ++i ) {
		p.board2[i][1] = pieces2::pawn;
		p.board2[i][6] = pieces2::pawn | (1 << 4);
	}

	for( unsigned int c = 0; c <= 1; ++c ) {
		p.castle[c] = 0x3;
	}

	p.can_en_passant = 0;

	p.material[0] = material_values::initial;
	p.material[1] = p.material[0];

	init_bitboards( p );

	p.init_pawn_structure();
}

extern unsigned long long pawn_control[2][64];

void init_bitboards( position& p )
{
	memset( p.bitboards, 0, sizeof(bitboard) * 2 );

	for( unsigned int col = 0; col < 8; ++col ) {
		for( unsigned int row = 0; row < 8; ++row ) {
			if( !p.board2[col][row] ) {
				continue;
			}

			color::type c = static_cast<color::type>(p.board2[col][row] >> 4);
			pieces2::type pi = static_cast<pieces2::type>(p.board2[col][row] & 0x0f);

			p.bitboards[c].b[pi] |= 1ull << (col + row * 8);
		}
	}

	for( int c = 0; c < 2; ++c ) {
		p.bitboards[c].b[bb_type::all_pieces] = p.bitboards[c].b[bb_type::pawns] | p.bitboards[c].b[bb_type::knights] | p.bitboards[c].b[bb_type::bishops] | p.bitboards[c].b[bb_type::rooks] | p.bitboards[c].b[bb_type::queens] | p.bitboards[c].b[bb_type::king];
	}
}

namespace {
static bool do_apply_move( position& p, move const& m, color::type c, bool& capture )
{
	unsigned long long const source_square = 1ull << (m.source_col + m.source_row * 8);
	unsigned long long const target_square = 1ull << (m.target_col + m.target_row * 8);

	capture = m.captured_piece != 0;

	p.board2[m.source_col][m.source_row] = 0;

	if( m.flags & move_flags::castle ) {
		if( m.target_col == 6 ) {
			// Kingside
			p.bitboards[c].b[bb_type::all_pieces] ^= 0xf0ull << (m.source_row * 8);
			p.bitboards[c].b[bb_type::king] ^= 0x50ull << (m.source_row * 8);
			p.bitboards[c].b[bb_type::rooks] ^= 0xa0ull << (m.source_row * 8);
			p.board2[6][m.source_row] = pieces2::king | (c << 4);
			p.board2[7][m.source_row] = 0;
			p.board2[5][m.source_row] = pieces2::rook | (c << 4);
		}
		else {
			// Queenside
			p.bitboards[c].b[bb_type::all_pieces] ^= 0x1dull << (m.source_row * 8);
			p.bitboards[c].b[bb_type::king] ^= 0x14ull << (m.source_row * 8);
			p.bitboards[c].b[bb_type::rooks] ^= 0x09ull << (m.source_row * 8);
			p.board2[2][m.source_row] = pieces2::king | (c << 4);
			p.board2[0][m.source_row] = 0;
			p.board2[3][m.source_row] = pieces2::rook | (c << 4);
		}
		p.can_en_passant = 0;
		p.castle[c] = 0x4;
		return true;
	}

	p.bitboards[c].b[m.piece] ^= source_square;
	p.bitboards[c].b[bb_type::all_pieces] ^= source_square;

	if( m.captured_piece != pieces2::none ) {
		if( m.flags & move_flags::enpassant ) {
			unsigned long long const ep_square = 1ull << (m.target_col + m.source_row * 8);
			p.bitboards[1-c].b[bb_type::pawns] ^= ep_square;
			p.bitboards[1-c].b[bb_type::all_pieces] ^= ep_square;
			p.board2[m.target_col][m.source_row] = 0;
		}
		else {
			p.bitboards[1-c].b[m.captured_piece] ^= target_square;
			p.bitboards[1-c].b[bb_type::all_pieces] ^= target_square;

			if( m.captured_piece == pieces2::rook ) {
				if( !m.target_col && m.target_row == (c ? 0 : 7) ) {
					p.castle[1-c] &= 0x5;
				}
				else if( m.target_col == 7 && m.target_row == (c ? 0 : 7) ) {
					p.castle[1-c] &= 0x6;
				}
			}
		}
	}

	if( m.piece == pieces2::rook ) {
		if( !m.source_col && m.source_row == (c ? 7 : 0) ) {
			p.castle[c] &= 0x5;
		}
		else if( m.source_col == 7 && m.source_row == (c ? 7 : 0) ) {
			p.castle[c] &= 0x6;
		}
	}
	else if( m.piece == pieces2::king ) {
		p.castle[c] &= 0x4;
	}

	if( m.piece == pieces2::pawn && (m.source_row + 2 == m.target_row || m.source_row == m.target_row + 2) ) {
		p.can_en_passant = (c << 6) + m.target_row * 8 + m.target_col;
	}
	else {
		p.can_en_passant = 0;
	}

	if( m.flags & move_flags::promotion ) {
		p.board2[m.target_col][m.target_row] = pieces2::queen | (c << 4);
		p.bitboards[c].b[bb_type::queens] ^= target_square;
	}
	else {
		p.board2[m.target_col][m.target_row] = m.piece | (c << 4);
		p.bitboards[c].b[m.piece] ^= target_square;
	}
	p.bitboards[c].b[bb_type::all_pieces] ^= target_square;
	
	return true;
}
}


bool apply_move( position& p, move const& m, color::type c, bool& capture )
{
	bool ret = do_apply_move( p, m, c, capture );

	if( m.piece == pieces2::pawn || m.captured_piece == pieces2::pawn ) {
		if( m.captured_piece == pieces2::pawn ) {
			if( m.flags & move_flags::enpassant ) {
				p.pawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(1-c), m.target_col, m.source_row );
			}
			else {
				p.pawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(1-c), m.target_col, m.target_row );
			}
		}
		if( m.piece == pieces2::pawn ) {
			p.pawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.source_col, m.source_row );
			if( !(m.flags & move_flags::promotion) ) {
				p.pawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.target_col, m.target_row );
			}
		}

		if( !pawn_hash_table.lookup( p.pawns.hash, p.pawns.eval ) ) {
			p.pawns.eval = evaluate_pawns(p.bitboards[0].b[bb_type::pawns], p.bitboards[1].b[bb_type::pawns]);
			pawn_hash_table.store( p.pawns.hash, p.pawns.eval );
		}
	}

	return ret;
}


bool apply_move( position& p, move_info const& mi, color::type c, bool& capture )
{
	bool ret = do_apply_move( p, mi.m, c, capture );
	p.pawns = mi.pawns;
	return ret;
}


namespace {
static mutex m;
static unsigned long long random_unsigned_long_long_pos = 0;
static unsigned long long random_unsigned_char = 0;
}

void init_random( unsigned long long seed )
{
	random_unsigned_char = seed;
	random_unsigned_long_long_pos = (seed + 0xf00) % sizeof(precomputed_random_data);
}


unsigned char get_random_unsigned_char()
{
	scoped_lock l( m ) ;
	if( ++random_unsigned_char >= sizeof(precomputed_random_data) ) {
		random_unsigned_char = 0;
	}
	return precomputed_random_data[random_unsigned_char];
}

unsigned long long get_random_unsigned_long_long()
{
	scoped_lock l( m );
	random_unsigned_long_long_pos += sizeof( unsigned long long );

	if( random_unsigned_long_long_pos >= (sizeof(precomputed_random_data) - 8) ) {
		random_unsigned_long_long_pos = 0;
	}

	unsigned long long ret = *reinterpret_cast<unsigned long long*>(precomputed_random_data + random_unsigned_long_long_pos);
	return ret;
}


void position::init_pawn_structure()
{
	pawns.hash = 0;
	for( int c = 0; c < 2; ++c ) {
		unsigned long long cpawns = bitboards[c].b[bb_type::pawns];
		while( cpawns ) {
			unsigned long long pawn;
			bitscan( cpawns, pawn );
			cpawns ^= 1ull << pawn;

			pawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), static_cast<unsigned char>(pawn % 8), static_cast<unsigned char>(pawn / 8) );
		}
	}
	pawns.eval = evaluate_pawns( bitboards[0].b[bb_type::pawns], bitboards[1].b[bb_type::pawns] );
}
