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
#include "tables.hpp"
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

	return validate_move( m, moves, pm );
}


bool validate_move( move const& m, move_info const* begin, move_info const* end )
{
	for( ; begin != end; ++begin ) {
		if( begin->m == m ) {
			return true;
		}
	}

	return false;
}


bool parse_move( position const& p, color::type c, std::string const& line, move& m, bool print_errors )
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
			if( print_errors ) {
				std::cout << "Illegal move (not valid): " << line << std::endl;
			}
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
			if( print_errors ) {
				std::cout << "Illegal move (not valid): " << line << std::endl;
			}
			return false;
		}
		return true;
	}


	unsigned char piecetype = 0;

	bool promotion = false;
	promotions::type promotion_type = promotions::queen;
	// Small b not in this list intentionally, to avoid disambiguation with a4xb
	if( len && (str[len - 1] == 'q' || str[len - 1] == 'Q' || str[len - 1] == 'r' || str[len - 1] == 'R' || str[len - 1] == 'B' || str[len - 1] == 'n' || str[len - 1] == 'N' ) ) {
		promotion = true;
		switch( str[len-1] ) {
			case 'q':
			case 'Q':
				promotion_type = promotions::queen;
				break;
			case 'r':
			case 'R':
				promotion_type = promotions::rook;
				break;
			case 'B':
				promotion_type = promotions::bishop;
				break;
			case 'n':
			case 'N':
				promotion_type = promotions::knight;
				break;
		}
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
		promotion_type = promotions::bishop;
		len -= 2;
		str = str.substr(0, len);
		piecetype = 'P';
	}
	else if( len == 5 && str[0] >= 'a' && str[0] <= 'h' && str[len - 1] == 'b' ) {
		// e7e8b
		promotion = true;
		promotion_type = promotions::bishop;
		--len;
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
			if( print_errors ) {
				std::cout << "Error (unknown command): " << line << std::endl;
			}
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
				if( print_errors ) {
					std::cout << "Error (unknown command): " << line << std::endl;
				}
				return false;
			}
			got_separator = true;
			if( *s == 'x' ) {
				capture = true;
			}
		}
		else if( *s >= 'a' && *s <= 'h' ) {
			if( second_col != -1 ) {
				if( print_errors ) {
					std::cout << "Error (unknown command): " << line << std::endl;
				}
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
				if( print_errors ) {
					std::cout << "Error (unknown command): " << line << std::endl;
				}
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
			if( print_errors ) {
				std::cout << "Error (unknown command): " << line << std::endl;
			}
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
		if( print_errors ) {
			std::cout << "Error (unknown command): " << line << std::endl;
		}
		return false;
	}

	check_map check;
	calc_check_map( p, c, check );

	move_info moves[200];
	move_info* pm = moves;
	calculate_moves( p, c, pm, check );

	std::list<move_info> matches;

	for( move_info* it = moves; it != pm; ++it ) {
		char source_piece = pieces::none;

		switch( it->m.piece ) {
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
		default:
			if( print_errors ) {
				std::cout << "Error (corrupt internal state): Got a move that does not have a source piece.";
			}
			return false;
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

		if( promotion && !(it->m.flags & move_flags::promotion) ) {
			continue;
		}
		if( promotion && it->m.promotion != promotion_type ) {
			continue;
		}

		matches.push_back(*it);
	}

	if( matches.size() > 1 ) {
		if( print_errors ) {
			std::cout << "Illegal move (ambigious): " << line << std::endl;
			std::cerr << "Candiates:" << std::endl;
			for( std::list<move_info>::const_iterator it = matches.begin(); it != matches.end(); ++it ) {
				std::cerr << move_to_string( it->m ) << std::endl;
			}
		}
		return false;
	}
	else if( matches.empty() ) {
		if( print_errors ) {
			std::cout << "Illegal move (not valid): " << line << std::endl;
			std::cerr << "Parsed:";
			if( first_col != -1 ) {
				std::cerr << " source_file=" << static_cast<char>('a' + first_col);
			}
			if( first_row != -1 ) {
				std::cerr << " source_rank=" << first_row;
			}
			if( second_col != -1 ) {
				std::cerr << " target_file=" << static_cast<char>('a' + second_col);
			}
			if( second_row != -1 ) {
				std::cerr << " target_rank=" << second_row;
			}
			if( promotion ) {
				std::cerr << " promotion=" << promotion_type << std::endl;
			}
			std::cerr << " capture=" << capture << std::endl;
		}
		return false;
	}

	move_info match = matches.front();
	if( promotion && match.m.target / 8 != (c ? 0 : 7) ) {
		if( print_errors ) {
			std::cout << "Illegal move (not valid, expecting a promotion): " << line << std::endl;
		}
		return false;
	}

	m = match.m;

	return true;
}


std::string move_to_string( move const& m, bool padding )
{
	std::string ret;

	if( m.flags & move_flags::castle ) {
		if( m.target == 6 || m.target == 62 ) {
			if( padding ) {
				return "   O-O  ";
			}
			else {
				return "O-O";
			}
		}
		else {
			if( padding ) {
				return " O-O-O  ";
			}
			else {
				return "O-O-O";
			}
		}
	}

	switch( m.piece ) {
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
			if( padding ) {
				ret += ' ';
			}
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
		switch( m.promotion ) {
			case promotions::knight:
				ret += "=N";
				break;
			case promotions::bishop:
				ret += "=B";
				break;
			case promotions::rook:
				ret += "=R";
				break;
			case promotions::queen:
				ret += "=Q";
				break;
		}
	}
	else if( padding ) {
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
	for( unsigned int c = 0; c <= 1; ++c ) {
		p.castle[c] = 0x3;
	}

	p.can_en_passant = 0;

	p.material[0] = eval_values.initial_material;
	p.material[1] = p.material[0];

	init_bitboards( p );

	p.init_pawn_structure();
}


void init_material( position& p ) {
	p.material[0] = 0;
	p.material[1] = 0;

	for( int c = 0; c < 2; ++c ) {
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			pieces::type b = get_piece_on_square( p, static_cast<color::type>(c), pi );

			if( b != pieces::king ) {
				p.material[c] += get_material_value( b );
			}
		}
	}
}

void position::clear_bitboards()
{
	memset( bitboards, 0, sizeof(bitboard) * 2 );
}

void init_bitboards( position& p )
{
	p.clear_bitboards();

	p.bitboards[color::white].b[bb_type::pawns]   = 0xff00ull;
	p.bitboards[color::white].b[bb_type::knights] = (1ull << 1) + (1ull << 6);
	p.bitboards[color::white].b[bb_type::bishops] = (1ull << 2) + (1ull << 5);
	p.bitboards[color::white].b[bb_type::rooks]   = 1ull + (1ull << 7);
	p.bitboards[color::white].b[bb_type::queens]  = 1ull << 3;
	p.bitboards[color::white].b[bb_type::king]    = 1ull << 4;

	p.bitboards[color::black].b[bb_type::pawns]   = 0xff000000000000ull;
	p.bitboards[color::black].b[bb_type::knights] = ((1ull << 1) + (1ull << 6)) << (7*8);
	p.bitboards[color::black].b[bb_type::bishops] = ((1ull << 2) + (1ull << 5)) << (7*8);
	p.bitboards[color::black].b[bb_type::rooks]   = (1ull + (1ull << 7)) << (7*8);
	p.bitboards[color::black].b[bb_type::queens]  = (1ull << 3) << (7*8);
	p.bitboards[color::black].b[bb_type::king]    = (1ull << 4) << (7*8);

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

	if( m.flags & move_flags::castle ) {
		unsigned char row = c ? 56 : 0;
		if( m.target % 8 == 6 ) {
			// Kingside
			p.bitboards[c].b[bb_type::all_pieces] ^= 0xf0ull << row;
			p.bitboards[c].b[bb_type::king] ^= 0x50ull << row;
			p.bitboards[c].b[bb_type::rooks] ^= 0xa0ull << row;
		}
		else {
			// Queenside
			p.bitboards[c].b[bb_type::all_pieces] ^= 0x1dull << row;
			p.bitboards[c].b[bb_type::king] ^= 0x14ull << row;
			p.bitboards[c].b[bb_type::rooks] ^= 0x09ull << row;
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
		p.bitboards[c].b[bb_type::knights + m.promotion] ^= target_square;

		p.material[c] -= get_material_value( pieces::pawn );
		p.material[c] += get_material_value( static_cast<pieces::type>(pieces::knight + m.promotion) );
	}
	else {
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

			pawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), static_cast<unsigned char>(pawn) );
		}
	}
	short pawn_eval[2];
	evaluate_pawns( bitboards[0].b[bb_type::pawns], bitboards[1].b[bb_type::pawns], pawn_eval );
	pawns.eval = phase_scale( material, pawn_eval[0], pawn_eval[1] );
}


bool position::is_occupied_square( unsigned long long square ) const {
	return get_occupancy( 1ull << square ) != 0;
}


unsigned long long position::get_occupancy( unsigned long long mask ) const
{
	return (bitboards[color::white].b[bb_type::all_pieces] | bitboards[color::black].b[bb_type::all_pieces]) & mask;
}


namespace {
static std::string board_square_to_string( position const& p, int pi )
{
	pieces::type piece;
	color::type c;
	if( p.bitboards[color::black].b[bb_type::all_pieces] & (1ull << pi) ) {
		c = color::black;
		piece = get_piece_on_square( p, c, pi );
	}
	else {
		c = color::white;
		piece = get_piece_on_square( p, c, pi );
	}

	if( c == color::white ) {
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
		switch (piece) {
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
		default:
			return ".";
		}
	}
	else {
		switch (piece) {
		case pieces::pawn:
			return "p";
		case pieces::knight:
			return "n";
		case pieces::bishop:
			return "b";
		case pieces::rook:
			return "r";
		case pieces::queen:
			return "q";
		case pieces::king:
			return "k";
		default:
			return ".";
		}
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
			ret += board_square_to_string( p, row * 8 + col );
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

pieces::type get_piece_on_square( position const& p, color::type c, unsigned long long square )
{
	pieces::type ret;
	if( p.bitboards[c].b[bb_type::all_pieces] & (1ull << square) ) {
		if( p.bitboards[c].b[bb_type::pawns] & (1ull << square) ) {
			ret = pieces::pawn;
		}
		else if( p.bitboards[c].b[bb_type::knights] & (1ull << square) ) {
			ret = pieces::knight;
		}
		else if( p.bitboards[c].b[bb_type::bishops] & (1ull << square) ) {
			ret = pieces::bishop;
		}
		else if( p.bitboards[c].b[bb_type::rooks] & (1ull << square) ) {
			ret = pieces::rook;
		}
		else if( p.bitboards[c].b[bb_type::queens] & (1ull << square) ) {
			ret = pieces::queen;
		}
		else if( p.bitboards[c].b[bb_type::king] & (1ull << square) ) {
			ret = pieces::king;
		}
		else {
			ret = pieces::none;
		}
	}
	else {
		ret = pieces::none;
	}

	return ret;
}

bool apply_hash_move( position& p, move const& m, color::type c, check_map const& check )
{
	if( m.piece != get_piece_on_square( p, c, m.source ) ) {
		return false;
	}
	if( m.captured_piece != get_piece_on_square( p, static_cast<color::type>(1-c), m.target ) ) {
		if( m.piece != pieces::pawn || m.captured_piece != pieces::pawn || !(m.flags & move_flags::enpassant) ) {
			return false;
		}
	}
	if( p.bitboards[c].b[bb_type::all_pieces] & (1ull << m.target) ) {
		return false;
	}

	if( m.piece == pieces::king ) {
		if( detect_check( p, c, m.target, m.source ) ) {
			return false;
		}
	}
	else {
		if( check.check && check.multiple() ) {
			return false;
		}

		unsigned char const& cv_old = check.board[m.source];
		unsigned char const& cv_new = check.board[m.target];
		if( check.check ) {
			if( cv_old ) {
				// Can't come to rescue, this piece is already blocking yet another check.
				return false;
			}
			if( cv_new != check.check ) {
				// Target position does capture checking piece nor blocks check
				return false;
			}
		}
		else {
			if( cv_old && cv_old != cv_new ) {
				return false;
			}
		}
	}

	return apply_move( p, m, c );
}
