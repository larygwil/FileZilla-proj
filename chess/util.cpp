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
#include <list>

extern unsigned char const queenside_rook_origin[2];
extern unsigned char const kingside_rook_origin[2];

bool validate_move( position const& p, move const& m, color::type c )
{
	check_map check;
	calc_check_map( p, c, check );

	move_info moves[200];
	move_info* pm = moves;
	calculate_moves( p, c, pm, check );

	for( move_info* it = moves; it != pm; ++it ) {
		if( it->m == m ) {
			return true;
		}
	}

	return false;
}

bool parse_move( position const& p, color::type c, std::string const& line, move& m )
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

	if( str == "0-0" || str == "O-O" ) {
		m.captured_piece = pieces::none;
		m.flags = move_flags::valid | move_flags::castle;
		m.piece = pieces::king;
		m.source = c ? 60 : 4;
		m.target = c ? 62 : 6;
		if( !validate_move( p, m, c ) ) {
			std::cout << "Illegal move (not valid): " << line << std::endl;
			return false;
		}
		return true;
	}
	else if( str == "0-0-0" || str == "O-O-O" ) {
		m.captured_piece = pieces::none;
		m.flags = move_flags::valid | move_flags::castle;
		m.piece = pieces::king;
		m.source = c ? 60 : 4;
		m.target = c ? 58 : 2;
		if( !validate_move( p, m, c ) ) {
			std::cout << "Illegal move (not valid): " << line << std::endl;
			return false;
		}
		return true;
	}


	unsigned char piecetype = 0;

	bool promotion = false;
	// Small b not in this list intentionally, to avoid disambiguation with a4xb
	if( len && (str[len - 1] == 'q' || str[len - 1] == 'Q' || str[len - 1] == 'r' || str[len - 1] == 'R' || str[len - 1] == 'B' || str[len - 1] == 'n' || str[len - 1] == 'N' ) ) {
		promotion = true;
		--len;
		str = str.substr(0, len);
		if( str[len - 1] == '=' ) {
			--len;
			str = str.substr(0, len);
		}
		piecetype = 'P';
	}
	else if( len > 2 && str[len - 1] == 'b' && str[len - 2] == '=' ) {
		promotion = true;
		len -= 2;
		str = str.substr(0, len);
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
	calculate_moves( p, c, pm, check );

	std::list<move_info> matches;

	for( move_info* it = moves; it != pm; ++it ) {
		unsigned char source = p.board[it->m.source] & 0xf;

		char source_piece = 0;

		switch( source ) {
		case pieces::king:
			source_piece = 'K';
			break;
		case pieces::queen:
			source_piece = 'Q';
			break;
		case pieces::rook:
			source_piece = 'R';
			break;
		case pieces::bishop:
			source_piece = 'B';
			break;
		case pieces::knight:
			source_piece = 'N';
			break;
		case pieces::pawn:
			source_piece = 'P';
			break;
		}

		if( piecetype && piecetype != source_piece ) {
			continue;
		}

		if( capture && !it->m.captured_piece ) {
			continue;
		}

		if( first_col != -1 && first_col != it->m.source % 8 ) {
			continue;
		}
		if( first_row != -1 && first_row != it->m.source / 8 ) {
			continue;
		}
		if( second_col != -1 && second_col != it->m.target % 8 ) {
			continue;
		}
		if( second_row != -1 && second_row != it->m.target / 8 ) {
			continue;
		}

		matches.push_back(*it);
	}

	if( matches.size() > 1 ) {
		std::cout << "Illegal move (ambigious): " << line << std::endl;
		std::cerr << "Candiates:" << std::endl;
		for( std::list<move_info>::const_iterator it = matches.begin(); it != matches.end(); ++it ) {
			std::cerr << move_to_string( p, c, it->m ) << std::endl;
		}
		return false;
	}
	else if( matches.empty() ) {
		std::cout << "Illegal move (not valid): " << line << std::endl;
		std::cerr << "Parsed:";
		if( first_col != -1 ) {
			std::cerr << " source_file=" << 'a' + first_col;
		}
		if( first_row != -1 ) {
			std::cerr << " source_rank=" << first_row;
		}
		if( second_col != -1 ) {
			std::cerr << " target_file=" << 'a' + second_col;
		}
		if( second_row != -1 ) {
			std::cerr << " target_rank=" << second_row;
		}
		std::cerr << " capture=" << capture << std::endl;
		return false;
	}

	move_info match = matches.front();
	if( promotion && match.m.target / 8 != (c ? 0 : 7) ) {
		std::cout << "Illegal move (not valid, expecting a promotion): " << line << std::endl;
		return false;
	}

	m = match.m;

	return true;
}


std::string move_to_string( position const& p, color::type c, move const& m )
{
	std::string ret;

	unsigned char source = p.board[m.source];

	if( (source >> 4 ) != c ) {
		std::cerr << "FAIL! Invalid move: "
				  << c << " "
				  << (source >> 4) << " "
				  << (source & 0xf) << " "
				  << static_cast<int>(m.source % 8) << " "
				  << static_cast<int>(m.source / 8) << " "
				  << static_cast<int>(m.target % 8) << " "
				  << static_cast<int>(m.target / 8) << " "
				  << std::endl;
	}

	source &= 0x0f;
	if( m.flags & move_flags::castle ) {
		if( m.target == (c ? 62 : 6 ) ) {
			return "   O-O  ";
		}
		else {
			return " O-O-O  ";
		}
	}

	switch( source ) {
		case pieces::bishop:
			ret += 'B';
			break;
		case pieces::knight:
			ret += 'N';
			break;
		case pieces::queen:
			ret += 'Q';
			break;
		case pieces::king:
			ret += 'K';
			break;
		case pieces::rook:
			ret += 'R';
			break;
		case pieces::pawn:
			ret += ' ';
			break;
		default:
			break;
	}

	ret += 'a' + m.source % 8;
	ret += '1' + m.source / 8;

	if( m.captured_piece ) {
		ret += 'x';
	}
	else {
		ret += '-';
	}

	ret += 'a' + m.target % 8;
	ret += '1' + m.target / 8;

	if( m.flags & move_flags::promotion ) {
		ret += "=Q";
	}
	else {
		ret += "  ";
	}

	return ret;
}


std::string move_to_source_target_string( move const& m )
{
	std::string ret;

	ret += 'a' + m.source % 8;
	ret += '1' + m.source / 8;
	ret += 'a' + m.target % 8;
	ret += '1' + m.target / 8;

	return ret;
}

void init_board( position& p )
{
	memset( p.board, 0, 64 );

	p.board[0] = pieces::rook;
	p.board[1] = pieces::knight;
	p.board[2] = pieces::bishop;
	p.board[3] = pieces::queen;
	p.board[4] = pieces::king;
	p.board[5] = pieces::bishop;
	p.board[6] = pieces::knight;
	p.board[7] = pieces::rook;

	p.board[56] = pieces::rook | (1 << 4);
	p.board[57] = pieces::knight | (1 << 4);
	p.board[58] = pieces::bishop | (1 << 4);
	p.board[59] = pieces::queen | (1 << 4);
	p.board[60] = pieces::king | (1 << 4);
	p.board[61] = pieces::bishop | (1 << 4);
	p.board[62] = pieces::knight | (1 << 4);
	p.board[63] = pieces::rook | (1 << 4);

	for( int i = 0; i < 8; ++i ) {
		p.board[i + 8] = pieces::pawn;
		p.board[i + 48] = pieces::pawn | (1 << 4);
	}

	for( unsigned int c = 0; c <= 1; ++c ) {
		p.castle[c] = 0x3;
	}

	p.can_en_passant = 0;

	p.material[0] = eval_values.initial_material;
	p.material[1] = p.material[0];

	init_bitboards( p );

	p.init_pawn_structure();
}

extern unsigned long long const pawn_control[2][64];

void init_material( position& p ) {
	p.material[0] = 0;
	p.material[1] = 0;

	for( unsigned int pi = 0; pi < 64; ++pi ) {
		if( !p.board[pi] ) {
			continue;
		}

		color::type c = static_cast<color::type>(p.board[pi] >> 4);
		pieces::type b = static_cast<pieces::type>(p.board[pi] & 0x0f);

		p.material[c] += get_material_value( b );
	}
}

void init_bitboards( position& p )
{
	memset( p.bitboards, 0, sizeof(bitboard) * 2 );

	for( unsigned int pi = 0; pi < 64; ++pi ) {
		if( !p.board[pi] ) {
			continue;
		}

		color::type c = static_cast<color::type>(p.board[pi] >> 4);
		pieces::type b = static_cast<pieces::type>(p.board[pi] & 0x0f);

		p.bitboards[c].b[b] |= 1ull << pi;
	}

	for( int c = 0; c < 2; ++c ) {
		p.bitboards[c].b[bb_type::all_pieces] = p.bitboards[c].b[bb_type::pawns] | p.bitboards[c].b[bb_type::knights] | p.bitboards[c].b[bb_type::bishops] | p.bitboards[c].b[bb_type::rooks] | p.bitboards[c].b[bb_type::queens] | p.bitboards[c].b[bb_type::king];

		p.bitboards[c].b[bb_type::pawn_control] = 0;
		unsigned long long pawn;
		unsigned long long pawns = p.bitboards[c].b[bb_type::pawns];
		while( pawns ) {
			bitscan( pawns, pawn );
			pawns &= pawns - 1;
			p.bitboards[c].b[bb_type::pawn_control] |= pawn_control[c][pawn];
		}
	}
}

namespace {
static bool do_apply_move( position& p, move const& m, color::type c )
{
	unsigned long long const source_square = 1ull << m.source;
	unsigned long long const target_square = 1ull << m.target;

	p.board[m.source] = 0;

	if( m.flags & move_flags::castle ) {
		unsigned char row = c ? 56 : 0;
		if( m.target % 8 == 6 ) {
			// Kingside
			p.bitboards[c].b[bb_type::all_pieces] ^= 0xf0ull << row;
			p.bitboards[c].b[bb_type::king] ^= 0x50ull << row;
			p.bitboards[c].b[bb_type::rooks] ^= 0xa0ull << row;
			p.board[6 + row] = pieces::king | (c << 4);
			p.board[7 + row] = 0;
			p.board[5 + row] = pieces::rook | (c << 4);
		}
		else {
			// Queenside
			p.bitboards[c].b[bb_type::all_pieces] ^= 0x1dull << row;
			p.bitboards[c].b[bb_type::king] ^= 0x14ull << row;
			p.bitboards[c].b[bb_type::rooks] ^= 0x09ull << row;
			p.board[2 + row] = pieces::king | (c << 4);
			p.board[0 + row] = 0;
			p.board[3 + row] = pieces::rook | (c << 4);
		}
		p.can_en_passant = 0;
		p.castle[c] = 0x4;
		return true;
	}

	p.bitboards[c].b[m.piece] ^= source_square;
	p.bitboards[c].b[bb_type::all_pieces] ^= source_square;

	if( m.captured_piece != pieces::none ) {
		if( m.flags & move_flags::enpassant ) {
			unsigned char ep = (m.target & 0x7) | (m.source & 0x38);
			unsigned long long const ep_square = 1ull << ep;
			p.bitboards[1-c].b[bb_type::pawns] ^= ep_square;
			p.bitboards[1-c].b[bb_type::all_pieces] ^= ep_square;
			p.board[ep] = 0;
		}
		else {
			p.bitboards[1-c].b[m.captured_piece] ^= target_square;
			p.bitboards[1-c].b[bb_type::all_pieces] ^= target_square;

			if( m.captured_piece == pieces::rook ) {
				if( m.target == queenside_rook_origin[1-c] ) {
					p.castle[1-c] &= 0x5;
				}
				else if( m.target == kingside_rook_origin[1-c] ) {
					p.castle[1-c] &= 0x6;
				}
			}
		}

		if( m.captured_piece == pieces::pawn ) {
			p.bitboards[1-c].b[bb_type::pawn_control] = 0;
			unsigned long long pawn;
			unsigned long long pawns = p.bitboards[1-c].b[bb_type::pawns];
			while( pawns ) {
				bitscan( pawns, pawn );
				pawns &= pawns - 1;
				p.bitboards[1-c].b[bb_type::pawn_control] |= pawn_control[1-c][pawn];
			}
		}
		p.material[1-c] -= get_material_value( m.captured_piece );
	}

	if( m.piece == pieces::rook ) {
		if( m.source == queenside_rook_origin[c] ) {
			p.castle[c] &= 0x5;
		}
		else if( m.source == kingside_rook_origin[c] ) {
			p.castle[c] &= 0x6;
		}
	}
	else if( m.piece == pieces::king ) {
		p.castle[c] &= 0x4;
	}

	if( m.flags & move_flags::pawn_double_move ) {
		p.can_en_passant = (m.target / 8 + m.source / 8) * 4 + m.target % 8;
	}
	else {
		p.can_en_passant = 0;
	}

	if( m.flags & move_flags::promotion ) {
		p.board[m.target] = pieces::queen | (c << 4);
		p.bitboards[c].b[bb_type::queens] ^= target_square;
	}
	else {
		p.board[m.target] = m.piece | (c << 4);
		p.bitboards[c].b[m.piece] ^= target_square;
	}
	p.bitboards[c].b[bb_type::all_pieces] ^= target_square;
	
	if( m.piece == pieces::pawn ) {
		p.bitboards[c].b[bb_type::pawn_control] = 0;
		unsigned long long pawn;
		unsigned long long pawns = p.bitboards[c].b[bb_type::pawns];
		while( pawns ) {
			bitscan( pawns, pawn );
			pawns &= pawns - 1;
			p.bitboards[c].b[bb_type::pawn_control] |= pawn_control[c][pawn];
		}
	}

	return true;
}
}


bool apply_move( position& p, move const& m, color::type c )
{
	bool ret = do_apply_move( p, m, c );

	if( m.piece == pieces::pawn || m.captured_piece == pieces::pawn ) {
		if( m.captured_piece == pieces::pawn ) {
			if( m.flags & move_flags::enpassant ) {
				unsigned char ep = (m.target & 0x7) | (m.source & 0x38);
				p.pawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(1-c), ep );
			}
			else {
				p.pawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(1-c), m.target );
			}
		}
		if( m.piece == pieces::pawn ) {
			p.pawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.source);
			if( !(m.flags & move_flags::promotion) ) {
				p.pawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.target );
			}
		}

		short pawn_eval[2];
		if( !pawn_hash_table.lookup( p.pawns.hash, pawn_eval ) ) {
			evaluate_pawns(p.bitboards[0].b[bb_type::pawns], p.bitboards[1].b[bb_type::pawns], pawn_eval);
			pawn_hash_table.store( p.pawns.hash, pawn_eval );
		}
		p.pawns.eval = phase_scale( p.material, pawn_eval[0], pawn_eval[1] );
	}

	return ret;
}


bool apply_move( position& p, move_info const& mi, color::type c )
{
	bool ret = do_apply_move( p, mi.m, c );
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
			cpawns &= cpawns - 1;

			pawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), pawn );
		}
	}
	short pawn_eval[2];
	evaluate_pawns( bitboards[0].b[bb_type::pawns], bitboards[1].b[bb_type::pawns], pawn_eval );
	pawns.eval = phase_scale( material, pawn_eval[0], pawn_eval[1] );
}


namespace {
static std::string board_square_to_string( char bs )
{
#if _MSC_VER
	std::string const white;
	std::string const restore;
#else
	std::string white;
	std::string restore;
	if( isatty( 0 ) ) {
		white = "\x1b" "[1m";
		restore = "\x1b" "[0m";
	}
#endif

	switch (bs) {
	case pieces::pawn:
		return white + "P" + restore;
	case pieces::knight:
		return white + "N" + restore;
	case pieces::bishop:
		return white + "B" + restore;
	case pieces::rook:
		return white + "R" + restore;
	case pieces::queen:
		return white + "Q" + restore;
	case pieces::king:
		return white + "K" + restore;
	case pieces::pawn + 16:
		return "p";
	case pieces::knight + 16:
		return "n";
	case pieces::bishop + 16:
		return "b";
	case pieces::rook + 16:
		return "r";
	case pieces::queen + 16:
		return "q";
	case pieces::king + 16:
		return "k";
	default:
		return ".";
	}
}
}


std::string board_to_string( position const& p )
{
	std::string ret;

	ret += " +-----------------+\n";
	for( int row = 7; row >= 0; --row ) {

		ret += '1' + row;
		ret += "| ";
		for( int col = 0; col < 8; ++col ) {
			ret += board_square_to_string( p.board[ row * 8 + col] );
			ret += ' ';
		}

		ret += "|\n";
	}
	ret += " +-----------------+\n";
	ret += "   A B C D E F G H \n";

	return ret;
}

static std::list<unsigned long long> stack;

void push_rng_state()
{
	scoped_lock l( m ) ;
	stack.push_back( random_unsigned_char );
	stack.push_back( random_unsigned_long_long_pos );
}

void pop_rng_state()
{
	scoped_lock l( m );
	if( stack.size() < 2 ) {
		std::cerr << "Cannot pop rng state if empty" << std::endl;
		exit(1);
	}

	random_unsigned_long_long_pos = stack.back();
	stack.pop_back();
	random_unsigned_char = stack.back();
	stack.pop_back();
}

