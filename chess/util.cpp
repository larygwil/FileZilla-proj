#include "chess.hpp"
#include "assert.hpp"
#include "calc.hpp"
#include "detect_check.hpp"
#include "eval.hpp"
#include "eval_values.hpp"
#include "fen.hpp"
#include "magic.hpp"
#include "moves.hpp"
#include "util.hpp"
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
	check_map check( p, c );

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
		m.flags = move_flags::castle;
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
		m.flags = move_flags::castle;
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

	pieces::type promotion = pieces::none;
	// Small b not in this list intentionally, to avoid disambiguation with a4xb
	if( len && (str[len - 1] == 'q' || str[len - 1] == 'Q' || str[len - 1] == 'r' || str[len - 1] == 'R' || str[len - 1] == 'B' || str[len - 1] == 'n' || str[len - 1] == 'N' ) ) {
		switch( str[len-1] ) {
			case 'q':
			case 'Q':
				promotion = pieces::queen;
				break;
			case 'r':
			case 'R':
				promotion = pieces::rook;
				break;
			case 'B':
				promotion = pieces::bishop;
				break;
			case 'n':
			case 'N':
				promotion = pieces::knight;
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
		promotion = pieces::bishop;
		len -= 2;
		str = str.substr(0, len);
		piecetype = 'P';
	}
	else if( len == 5 && str[0] >= 'a' && str[0] <= 'h' && str[len - 1] == 'b' ) {
		// e7e8b
		promotion = pieces::bishop;
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

	if( !piecetype && (first_col == -1 || second_col == -1 || first_row == -1 || second_row == -1) ) {
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

	check_map check( p, c );

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

		if( promotion != pieces::none && promotion != ((it->m.flags & move_flags::promotion_mask) >> move_flags::promotion_shift) ) {
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
			if( promotion != pieces::none ) {
				std::cerr << " promotion=" << static_cast<int>(promotion) << std::endl;
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

	int promotion = m.flags & move_flags::promotion_mask;
	if( promotion ) {
		switch( promotion ) {
			case move_flags::promotion_knight:
				ret += "=N";
				break;
			case move_flags::promotion_bishop:
				ret += "=B";
				break;
			case move_flags::promotion_rook:
				ret += "=R";
				break;
			case move_flags::promotion_queen:
				ret += "=Q";
				break;
		}
	}
	else if( padding ) {
		ret += "  ";
	}

	return ret;
}


std::string move_to_long_algebraic( move const& m )
{
	std::string ret;

	ret += 'a' + m.source % 8;
	ret += '1' + m.source / 8;
	ret += 'a' + m.target % 8;
	ret += '1' + m.target / 8;

	unsigned char promotion = m.flags & move_flags::promotion_mask;
	switch( promotion ) {
	case move_flags::promotion_queen:
		ret += 'q';
		break;
	case move_flags::promotion_rook:
		ret += 'r';
		break;
	case move_flags::promotion_bishop:
		ret += 'b';
		break;
	case move_flags::promotion_knight:
		ret += 'n';
		break;
	default:
		break;
	}

	return ret;
}

void init_board( position& p )
{
	for( unsigned int c = 0; c <= 1; ++c ) {
		p.castle[c] = 0x3;
	}

	p.can_en_passant = 0;

	init_bitboards( p );

	p.update_derived();
}


void position::update_derived()
{
	for( int i = 0; i < 2; ++i ) {
		bitboards[i].b[bb_type::all_pieces] = bitboards[i].b[bb_type::pawns] | bitboards[i].b[bb_type::knights] | bitboards[i].b[bb_type::bishops] | bitboards[i].b[bb_type::rooks] | bitboards[i].b[bb_type::queens] | bitboards[i].b[bb_type::king];

		bitboards[i].b[bb_type::pawn_control] = 0;
		uint64_t pawns = bitboards[i].b[bb_type::pawns];
		while( pawns ) {
			uint64_t pawn = bitscan_unset( pawns );
			bitboards[i].b[bb_type::pawn_control] |= pawn_control[i][pawn];
		}

		king_pos[i] = static_cast<int>(bitscan( bitboards[i].b[bb_type::king] ));
	}

	init_material();
	init_eval();
	init_pawn_hash();
}


void position::init_material()
{
	material[0] = score();
	material[1] = score();

	for( int c = 0; c < 2; ++c ) {
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			pieces::type b = get_piece_on_square( *this, static_cast<color::type>(c), pi );

			if( b != pieces::king ) {
				material[c] += eval_values::material_values[b];
			}
		}
	}
}


void position::init_eval()
{
	base_eval = material[0] - material[1];

	for( int c = 0; c < 2; ++c ) {
		score side;
		for( unsigned int sq = 0; sq < 64; ++sq ) {
			pieces::type pi = get_piece_on_square( *this, static_cast<color::type>(c), sq );

			side += pst[c][pi][sq];
		}

		if( c ) {
			base_eval -= side;
		}
		else {
			base_eval += side;
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
}

void apply_move( position& p, move const& m, color::type c )
{
	uint64_t const source_square = 1ull << m.source;
	uint64_t const target_square = 1ull << m.target;

	score delta = -pst[c][m.piece][m.source];

	if( m.flags & move_flags::castle ) {
		p.king_pos[c] = m.target;
		delta += pst[c][pieces::king][m.target];

		unsigned char row = c ? 56 : 0;
		if( m.target % 8 == 6 ) {
			// Kingside
			p.bitboards[c].b[bb_type::all_pieces] ^= 0xf0ull << row;
			p.bitboards[c].b[bb_type::king] ^= 0x50ull << row;
			p.bitboards[c].b[bb_type::rooks] ^= 0xa0ull << row;

			delta += pst[c][pieces::rook][row + 5] - pst[c][pieces::rook][row + 7];
		}
		else {
			// Queenside
			p.bitboards[c].b[bb_type::all_pieces] ^= 0x1dull << row;
			p.bitboards[c].b[bb_type::king] ^= 0x14ull << row;
			p.bitboards[c].b[bb_type::rooks] ^= 0x09ull << row;
			delta += pst[c][pieces::rook][row + 3] - pst[c][pieces::rook][row];
		}
		p.can_en_passant = 0;
		p.castle[c] = 0x4;

		if( c ) {
			p.base_eval -= delta;
		}
		else {
			p.base_eval += delta;
		}

		ASSERT( p.verify() );
		return;
	}

	p.bitboards[c].b[m.piece] ^= source_square;
	p.bitboards[c].b[bb_type::all_pieces] ^= source_square;

	if( m.captured_piece != pieces::none ) {
		if( m.flags & move_flags::enpassant ) {
			unsigned char ep = (m.target & 0x7) | (m.source & 0x38);
			uint64_t const ep_square = 1ull << ep;
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
			uint64_t pawns = p.bitboards[1-c].b[bb_type::pawns];
			while( pawns ) {
				uint64_t pawn = bitscan_unset( pawns );
				p.bitboards[1-c].b[bb_type::pawn_control] |= pawn_control[1-c][pawn];
			}

			if( m.flags & move_flags::enpassant ) {
				unsigned char ep = (m.target & 0x7) | (m.source & 0x38);
				p.pawn_hash ^= get_pawn_structure_hash( static_cast<color::type>(1-c), ep );
				delta += pst[1-c][pieces::pawn][ep] + eval_values::material_values[ pieces::pawn ];
			}
			else {
				p.pawn_hash ^= get_pawn_structure_hash( static_cast<color::type>(1-c), m.target );
				delta += pst[1-c][pieces::pawn][m.target] + eval_values::material_values[ pieces::pawn ];
			}
		}
		else {
			delta += pst[1-c][m.captured_piece][m.target] + eval_values::material_values[ m.captured_piece ];
		}
		p.material[1-c] -= eval_values::material_values[ m.captured_piece ];
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
		p.king_pos[c] = m.target;
	}

	if( m.flags & move_flags::pawn_double_move ) {
		p.can_en_passant = (m.target / 8 + m.source / 8) * 4 + m.target % 8;
	}
	else {
		p.can_en_passant = 0;
	}

	int promotion = m.flags & move_flags::promotion_mask;
	if( promotion ) {
		pieces::type promotion_piece = static_cast<pieces::type>(promotion >> move_flags::promotion_shift);

		p.bitboards[c].b[promotion_piece] ^= target_square;

		p.material[c] -= eval_values::material_values[ pieces::pawn ];
		p.material[c] += eval_values::material_values[ promotion_piece ];

		delta -= eval_values::material_values[pieces::pawn];
		delta += eval_values::material_values[promotion_piece] + pst[c][promotion_piece][m.target];
	}
	else {
		p.bitboards[c].b[m.piece] ^= target_square;
		delta += pst[c][m.piece][m.target];
	}
	p.bitboards[c].b[bb_type::all_pieces] ^= target_square;
	
	if( m.piece == pieces::pawn ) {
		p.bitboards[c].b[bb_type::pawn_control] = 0;
		uint64_t pawns = p.bitboards[c].b[bb_type::pawns];
		while( pawns ) {
			uint64_t pawn = bitscan_unset( pawns );
			p.bitboards[c].b[bb_type::pawn_control] |= pawn_control[c][pawn];
		}

		p.pawn_hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.source);
		if( !(m.flags & move_flags::promotion_mask) ) {
			p.pawn_hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.target );
		}
	}

	if( c ) {
		p.base_eval -= delta;
	}
	else {
		p.base_eval += delta;
	}

	ASSERT( p.verify() );
}


void position::init_pawn_hash()
{
	pawn_hash = 0;
	for( int c = 0; c < 2; ++c ) {
		uint64_t cpawns = bitboards[c].b[bb_type::pawns];
		while( cpawns ) {
			uint64_t pawn = bitscan_unset( cpawns );
			pawn_hash ^= get_pawn_structure_hash( static_cast<color::type>(c), static_cast<unsigned char>(pawn) );
		}
	}
}


bool position::is_occupied_square( uint64_t square ) const {
	return get_occupancy( 1ull << square ) != 0;
}


uint64_t position::get_occupancy( uint64_t mask ) const
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
#if WINDOWS
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

pieces::type get_piece_on_square( position const& p, color::type c, uint64_t square )
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
#if 0
	if( !is_valid_move( p, c, m, check ) ) {
		return false;
	}
#else
	(void)check;
#endif

	apply_move( p, m, c );

	return true;
}

bool do_is_valid_move( position const& p, color::type c, move const& m, check_map const& check )
{
	// Must move own piece
	if( m.piece != get_piece_on_square( p, c, m.source ) ) {
		return false;
	}

	// Must move onto square not occupied by self
	if( p.bitboards[c].b[bb_type::all_pieces] & (1ull << m.target) ) {
		return false;
	}

	if( m.piece == pieces::king ) {

		if( m.captured_piece != get_piece_on_square( p, static_cast<color::type>(1-c), m.target ) ) {
			return false;
		}

		uint64_t other_kings = p.bitboards[1-c].b[bb_type::king];
		uint64_t other_king = bitscan( other_kings );
		if( (1ull << m.target) & possible_king_moves[other_king] ) {
			return false;
		}

		if( m.flags & move_flags::castle ) {
			if( check.check ) {
				return false;
			}
			if( (m.target & 0x07) == 2 ) {
				if( !(p.castle[c] & castles::queenside) ) {
					return false;
				}
				if( !(p.bitboards[c].b[bb_type::rooks] & (1ull << (m.source - 4))) ) {
					return false;
				}
				if( (p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]) & (7ull << (m.source - 3 )) ) {
					return false;
				}
				if( detect_check( p, c, m.source - 1, m.source ) ) {
					return false;
				}
				if( detect_check( p, c, m.source - 2, m.source ) ) {
					return false;
				}
			}
			else {
				if( !(p.castle[c] & castles::kingside) ) {
					return false;
				}
				if( !(p.bitboards[c].b[bb_type::rooks] & (1ull << (m.source + 3))) ) {
					return false;
				}
				if( (p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]) & (3ull << (m.source + 1 )) ) {
					return false;
				}
				if( detect_check( p, c, m.source + 1, m.source ) ) {
					return false;
				}
			}
		}

		if( detect_check( p, c, m.target, m.source ) ) {
			return false;
		}
	}
	else {
		if( check.check && check.multiple() ) {
			return false;
		}

		if( m.flags & move_flags::enpassant ) {
			if( m.piece != pieces::pawn || m.captured_piece != pieces::pawn ) {
				return false;
			}

			if( p.can_en_passant != m.target ) {
				return false;
			}

			if( get_piece_on_square( p, static_cast<color::type>(1-c), m.target % 8 + (m.source & 0xf8) ) != pieces::pawn ) {
				return false;
			}

			unsigned char new_col = m.target % 8;
			unsigned char old_col = m.source % 8;
			unsigned char old_row = m.source / 8;

			unsigned char const& cv_old = check.board[m.source];
			unsigned char const& cv_new = check.board[m.target];
			if( check.check ) {
				if( cv_old ) {
					// Can't come to rescue, this piece is already blocking yet another check.
					return false;
				}
				if( cv_new != check.check && check.check != (0x80 + new_col + old_row * 8) ) {
					// Target position does capture checking piece nor blocks check
					return false;
				}
			}
			else {
				if( cv_old && cv_old != cv_new ) {
					return false;
				}
			}

			// Special case: black queen, black pawn, white pawn, white king from left to right on rank 5. Capturing opens up check!
			unsigned char king_col = static_cast<unsigned char>(p.king_pos[c] % 8);
			unsigned char king_row = static_cast<unsigned char>(p.king_pos[c] / 8);

			if( king_row == old_row ) {
				signed char cx = static_cast<signed char>(old_col) - king_col;
				if( cx > 0 ) {
					cx = 1;
				}
				else {
					cx = -1;
				}
				for( signed char col = old_col + cx; col < 8 && col >= 0; col += cx ) {
					if( col == new_col ) {
						continue;
					}

					if( p.bitboards[c].b[bb_type::all_pieces] & (1ull << (col + old_row * 8 ) ) ) {
						// Own piece
						continue;
					}

					pieces::type t = get_piece_on_square( p, static_cast<color::type>(1-c), col + old_row * 8 );
					if( t == pieces::queen || t == pieces::rook ) {
						// Not a legal move unfortunately
						return false;
					}

					// Harmless piece
					break;
				}
			}
		}
		else {

			if( m.captured_piece != get_piece_on_square( p, static_cast<color::type>(1-c), m.target ) ) {
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

			if( m.piece == pieces::bishop ) {
				uint64_t const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];
				uint64_t possible_moves = bishop_magic( m.source, all_blockers );
				if( !(possible_moves & (1ull << m.target ) ) ) {
					return false;
				}
			}
			else if( m.piece == pieces::rook ) {
				uint64_t const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];
				uint64_t possible_moves = rook_magic( m.source, all_blockers );
				if( !(possible_moves & (1ull << m.target ) ) ) {
					return false;
				}
			}
			else if( m.piece == pieces::queen) {
				uint64_t const all_blockers = p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces];
				uint64_t possible_moves = rook_magic( m.source, all_blockers ) | bishop_magic( m.source, all_blockers );
				if( !(possible_moves & (1ull << m.target ) ) ) {
					return false;
				}
			}
			else if( m.piece == pieces::pawn ) {
				if( m.flags & move_flags::pawn_double_move ) {
					if( c == color::white ) {
						if( (p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]) & (1ull << (m.source + 8) ) ) {
							return false;
						}
					}
					else {
						if( (p.bitboards[c].b[bb_type::all_pieces] | p.bitboards[1-c].b[bb_type::all_pieces]) & (1ull << (m.source - 8) ) ) {
							return false;
						}
					}
				}
			}
		}
	}

	return true;
}

bool is_valid_move( position const& p, color::type c, move const& m, check_map const& check )
{
	bool ret = do_is_valid_move( p, c, m, check );

#if 0
	move_info moves[200];
	move_info* it = moves;
	move_info* end = moves;
	calculate_moves( p, c, end, check );
	for( ; it != end; ++it ) {
		if( it->m == m ) {
			if( ret ) {
				return true;
			}
			else {
				break;
			}
		}
	}
	if( ret || it != end ) {
		std::cerr << board_to_string( p ) << std::endl;
		std::cerr << position_to_fen_noclock( p, c ) << std::endl;
		std::cerr << move_to_string( m ) << std::endl;
		std::cerr << "Ret: " << ret << std::endl;
		abort();
	}
#endif

	return ret;
}

bool position::verify() const {
	position p2 = *this;
	p2.update_derived();
	if( base_eval != p2.base_eval ) {
		std::cerr << "FAIL BASE EVAL!" << std::endl;
		return false;
	}
	if( pawn_hash != p2.pawn_hash ) {
		std::cerr << "PAWN HASH FAIL" << std::endl;
		return false;
	}
	if( material[0] != p2.material[0] || material[1] != p2.material[1] ) {
		std::cerr << "Material mismatch!" << std::endl;
		return false;
	}
	for( int c = 0; c < 1; ++c ) {
		if( bitboards[c].b[bb_type::all_pieces] != p2.bitboards[c].b[bb_type::all_pieces] ) {
			std::cerr << "Bitboard error: Wron all pieces" << std::endl;
			return false;
		}
		if( bitboards[c].b[bb_type::pawn_control] != p2.bitboards[c].b[bb_type::pawn_control] ) {
			std::cerr << "Bitboard error: Wrong pawn control!" << std::endl;
			return false;
		}
	}

	return true;
}
