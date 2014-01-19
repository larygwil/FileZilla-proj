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
#include "tables.hpp"
#include "util/logger.hpp"
#include "zobrist.hpp"

#include <iostream>
#include <list>

bool validate_move( position const& p, move const& m )
{
	check_map check( p );

	move_info moves[200];
	move_info* pm = moves;
	calculate_moves<movegen_type::all>( p, pm, check );

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


bool parse_move( position const& p, std::string const& line, move& m, std::string& error )
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
		m = move(p.king_pos[p.self()], p.white() ? 6 : 62, move_flags::castle);
		if( !validate_move( p, m ) ) {
			error = "Illegal move (not valid)";
			return false;
		}
		return true;
	}
	else if( str == "0-0-0" || str == "O-O-O" ) {
		m = move(p.king_pos[p.self()], p.white() ? 2 : 58, move_flags::castle);
		if( !validate_move(p, m) ) {
			error = "Illegal move (not valid)";
			return false;
		}
		return true;
	}


	pieces::type piecetype = pieces::none;
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
		piecetype = pieces::pawn;
	}
	else if( len > 2 && str[len - 1] == 'b' && str[len - 2] == '=' ) {
		promotion = pieces::bishop;
		len -= 2;
		str = str.substr(0, len);
		piecetype = pieces::pawn;
	}
	else if( len == 5 && str[0] >= 'a' && str[0] <= 'h' && str[len - 1] == 'b' ) {
		// e7e8b
		promotion = pieces::bishop;
		--len;
		str = str.substr(0, len);
		piecetype = pieces::pawn;
	}

	const char* s = str.c_str();

	pieces::type newType;
	switch( *s ) {
	case 'P':
		newType = pieces::pawn;
		break;
	case 'N':
		newType = pieces::knight;
		break;
	case 'B':
		newType = pieces::bishop;
		break;
	case 'R':
		newType = pieces::rook;
		break;
	case 'Q':
		newType = pieces::queen;
		break;
	case 'K':
		newType = pieces::king;
		break;
	default:
		newType = pieces::none;
		break;
	}
	if( newType != pieces::none ) {
		++s;
		if( piecetype != pieces::none ) {
			error = "Error (unknown command)";
			return false;
		}
		piecetype = newType;
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
				error = "Error (unknown command)";
				return false;
			}
			got_separator = true;
			if( *s == 'x' ) {
				capture = true;
			}
		}
		else if( *s >= 'a' && *s <= 'h' ) {
			if( second_col != -1 ) {
				error = "Error (unknown command)";
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
				error = "Error (unknown command)";
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
			error = "Error (unknown command)";
			return false;
		}

		++s;
	}

	if( piecetype == pieces::none && (first_col == -1 || second_col == -1 || first_row == -1 || second_row == -1) ) {
		piecetype = pieces::pawn;
	}

	if( !got_separator && second_col == -1 && second_row == -1 ) {
		second_col = first_col;
		first_col = -1;
		second_row = first_row;
		first_row = -1;
	}

	if( first_col == -1 && first_row == -1 && second_col == -1 && second_row == -1 ) {
		error = "Error (unknown command)";
		return false;
	}

	check_map check( p );

	move_info moves[200];
	move_info* pm = moves;
	calculate_moves_by_piece( p, pm, check, piecetype );

	std::list<move> matches;

	// In Fischer Random chess, castling may be represented by king capturing own rook.
	if( (piecetype == pieces::none || piecetype == pieces::king) && second_col >= 0 && second_row == (p.c ? 7 : 0) && p.get_piece_with_color(second_col + second_row 
* 8) == (p.white() ? pieces_with_color::white_rook : pieces_with_color::black_rook) ) {

		if( p.castle[p.self()] & (1ull << second_col)  ) {

			bool kingside = second_col > static_cast<int>(p.king_pos[p.self()] % 8);
			uint64_t target = second_row * 8 + (kingside ? 6 : 2);

			for( move_info* it = moves; it != pm; ++it ) {
				if( !it->m.castle() ) {
					continue;
				}
				if( first_col != -1 && first_col != it->m.source() % 8 ) {
					continue;
				}
				if( first_row != -1 && first_row != it->m.source() / 8 ) {
					continue;
				}			
				if( it->m.target() != target ) {
					continue;
				}

				matches.push_back( it->m );
			}
		}
	}

	for( move_info* it = moves; it != pm; ++it ) {
		pieces::type piece = p.get_piece(it->m.source());

		if( piecetype != pieces::none && piecetype != piece ) {
			continue;
		}

		pieces::type captured_piece = p.get_captured_piece( it->m );
		if( capture && !captured_piece ) {
			continue;
		}

		if( first_col != -1 && first_col != it->m.source() % 8 ) {
			continue;
		}
		if( first_row != -1 && first_row != it->m.source() / 8 ) {
			continue;
		}
		if( second_col != -1 && second_col != it->m.target() % 8 ) {
			continue;
		}
		if( second_row != -1 && second_row != it->m.target() / 8 ) {
			continue;
		}

		if( promotion != pieces::none && (!it->m.promotion() || it->m.promotion_piece() != promotion) ) {
			continue;
		}

		matches.push_back(it->m);
	}

	// In Fischer-Random chess, a king move and a castling may have the very same source and target squares.
	if( matches.size() == 2 ) {
		move m1 = matches.front();
		move m2 = matches.back();
		if( (m1.castle() || m2.castle()) && m1.source() == m2.source() && m1.target() == m2.target() ) {
			matches.clear();
			matches.push_back( m1.castle() ? m2 : m1 );
		}
	}

	if( matches.size() > 1 ) {
		error = "Illegal move (ambigious)";
		dlog() << "Candiates:" << std::endl;
		for( auto it : matches ) {
			dlog() << move_to_string( p, it ) << std::endl;
		}
		return false;
	}
	else if( matches.empty() ) {
		error = "Illegal move (not valid)";
		dlog() << "Parsed:";
		if( first_col != -1 ) {
			dlog() << " source_file=" << static_cast<char>('a' + first_col);
		}
		if( first_row != -1 ) {
			dlog() << " source_rank=" << first_row;
		}
		if( second_col != -1 ) {
			dlog() << " target_file=" << static_cast<char>('a' + second_col);
		}
		if( second_row != -1 ) {
			dlog() << " target_rank=" << second_row;
		}
		if( promotion != pieces::none ) {
			dlog() << " promotion=" << static_cast<int>(promotion) << std::endl;
		}
		dlog() << " capture=" << capture << std::endl;
		return false;
	}

	move match = matches.front();
	if( promotion && match.target() / 8 != (p.white() ? 7 : 0) ) {
		error = "Illegal move (not valid, expecting a promotion)";
		return false;
	}

	if( p.get_captured_piece( match ) == pieces::king ) {
		error = "Illegal move (cannot capture king)";
		return false;
	}

	m = match;

	return true;
}


std::string move_to_string( position const& p, move const& m, bool padding )
{
	std::string ret;

	if( m.castle() ) {
		if( (m.target() % 8) == 6 ) {
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

	pieces::type piece = p.get_piece(m.source());
	switch( piece ) {
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

	ret += 'a' + m.source() % 8;
	ret += '1' + m.source() / 8;

	pieces::type captured_piece = p.get_captured_piece( m );
	if( captured_piece ) {
		ret += 'x';
	}
	else {
		ret += '-';
	}

	ret += 'a' + m.target() % 8;
	ret += '1' + m.target() / 8;

	if( m.promotion() ) {
		switch( m.promotion_piece() ) {
			case pieces::knight:
				ret += "=N";
				break;
			case pieces::bishop:
				ret += "=B";
				break;
			case pieces::rook:
				ret += "=R";
				break;
			case pieces::queen:
			default:
				ret += "=Q";
				break;
		}
	}
	else if( padding ) {
		ret += "  ";
	}

	return ret;
}


namespace {
void add_disambiguation( position const& p, move const& m, uint64_t possible_moves, std::string& ret ) {
	uint64_t source_file = 0x0101010101010101ull << (m.source() % 8);
	uint64_t source_rank = 0x00000000000000ffull << (m.source() & 0x38);
	pieces::type piece = p.get_piece( m );
	pieces::type captured_piece = p.get_captured_piece( m );
	if( popcount( possible_moves & p.bitboards[p.self()][piece] ) > 1 ) {
		if( popcount(p.bitboards[p.self()][piece] & source_file) == 1 ) {
			ret += 'a' + m.source() % 8;
		}
		else if( popcount(p.bitboards[p.self()][piece] & source_rank) == 1 ) {
			ret += '1' + m.source() / 8;
		}
		else {
			ret += 'a' + m.source() % 8;
			ret += '1' + m.source() / 8;
		}
	}
	if( captured_piece != pieces::none ) {
		ret += 'x';
	}
	ret += 'a' + m.target() % 8;
	ret += '1' + m.target() / 8;
}
}

std::string move_to_san( position const& p, move const& m )
{
	std::string ret;

	if( m.empty() ) {
		return "";
	}

	if( m.castle() ) {
		if( (m.target() % 8) == 6 ) {
			return "O-O";
		}
		else {
			return "O-O-O";
		}
	}

	color::type c;
	if( p.bitboards[color::white][bb_type::all_pieces] & (1ull << m.source()) ) {
		c = color::white;
	}
	else {
		c = color::black;
	}

	pieces::type piece = p.get_piece( m.source() );
	pieces::type captured_piece = p.get_captured_piece( m );

	switch( piece ) {
		case pieces::knight:
			ret += 'N';
			add_disambiguation( p, m, possible_knight_moves[m.target()], ret );
			break;
		case pieces::bishop:
			ret += 'B';
			add_disambiguation( p, m, bishop_magic( m.target(), p.bitboards[c][bb_type::all_pieces] | p.bitboards[other(c)][bb_type::all_pieces] ), ret );
			break;
		case pieces::rook:
			ret += 'R';
			add_disambiguation( p, m, rook_magic( m.target(), p.bitboards[c][bb_type::all_pieces] | p.bitboards[other(c)][bb_type::all_pieces] ), ret );
			break;
		case pieces::queen:
			ret += 'Q';
			add_disambiguation( p, m, bishop_magic( m.target(), p.bitboards[c][bb_type::all_pieces] | p.bitboards[other(c)][bb_type::all_pieces] ) | rook_magic( m.target(), p.bitboards[c][bb_type::all_pieces] | p.bitboards[other(c)][bb_type::all_pieces] ), ret );
			break;
		case pieces::king:
			ret += 'K';
			if( captured_piece != pieces::none ) {
				ret += 'x';
			}
			ret += 'a' + m.target() % 8;
			ret += '1' + m.target() / 8;
			break;
		case pieces::pawn:
			if( captured_piece != pieces::none || m.enpassant() ) {
				ret += 'a' + m.source() % 8;
				ret += 'x';
				ret += 'a' + m.target() % 8;

				uint64_t target_file = 0x0101010101010101ull << (m.target() % 8);
				if( popcount(p.bitboards[c][bb_type::pawn_control] & target_file & p.bitboards[other(c)][bb_type::all_pieces]) > 1 ) {
					ret += '1' + m.target() / 8;
				}
			}
			else {
				ret += 'a' + m.target() % 8;
				ret += '1' + m.target() / 8;
			}

			{
				if( m.promotion() ) {
					switch( m.promotion_piece() ) {
						case pieces::knight:
							ret += "=N";
							break;
						case pieces::bishop:
							ret += "=B";
							break;
						case pieces::rook:
							ret += "=R";
							break;
						case pieces::queen:
						default:
							ret += "=Q";
							break;
					}
				}
			}
			break;
		default:
			break;
	}

	return ret;
}


std::string move_to_long_algebraic( config const& conf, position const& p, move const& m )
{
	std::string ret;

	if( !m.empty() ) {
		ret += 'a' + m.source() % 8;
		ret += '1' + m.source() / 8;

		if( conf.fischer_random && m.castle() ) {
			bool kingside = (m.target() % 8) == 6;
			uint64_t rook = kingside ? bitscan_reverse(p.castle[p.c]) : bitscan(p.castle[p.c]);
			ret += 'a' + static_cast<unsigned char>(rook);
			ret += '1' + m.target() / 8;

		}
		else {
			ret += 'a' + m.target() % 8;
			ret += '1' + m.target() / 8;

			if( m.promotion() ) {
				switch( m.promotion_piece() ) {
					case pieces::queen:
					default:
						ret += 'q';
						break;
					case pieces::rook:
						ret += 'r';
						break;
					case pieces::bishop:
						ret += 'b';
						break;
					case pieces::knight:
						ret += 'n';
						break;
				}
			}
		}
	}

	return ret;
}

void apply_move( position& p, move const& m )
{
	uint64_t const source_square = 1ull << m.source();
	uint64_t const target_square = 1ull << m.target();

	pieces_with_color::type pwc = p.get_piece_with_color( m.source() );
	pieces::type piece = get_piece( pwc );
	pieces::type captured_piece = p.get_captured_piece( m );

	if( piece == pieces::pawn || captured_piece != pieces::none ) {
		p.halfmoves_since_pawnmove_or_capture = 0;
	}
	else {
		++p.halfmoves_since_pawnmove_or_capture;
	}

	p.hash_ = ~p.hash_;
	p.hash_ ^= zobrist::enpassant[p.can_en_passant];
	p.hash_ ^= get_piece_hash( piece, p.self(), m.source() );

	score delta = -pst(p.self(), piece, m.source());

	if( m.castle() ) {
		p.king_pos[p.self()] = static_cast<square::type>(m.target());
		delta += pst(p.self(), pieces::king, m.target());

		p.hash_ ^= get_piece_hash( piece, p.self(), m.target() );
		p.hash_ ^= zobrist::castle[p.self()][p.castle[p.self()]];

		bool kingside = (m.target() % 8) == 6;

		uint64_t rook = kingside ? bitscan_reverse(p.castle[p.c]) : bitscan(p.castle[p.c]);

		unsigned char row = p.white() ? 0 : 56;

		// Source squares
		p.bitboards[p.self()][bb_type::all_pieces] ^= 1ull << m.source();
		p.bitboards[p.self()][bb_type::king] ^= 1ull << m.source();
		p.bitboards[p.self()][bb_type::all_pieces] ^= 1ull << (row + rook);
		p.bitboards[p.self()][bb_type::rooks] ^= 1ull << (row + rook);

		p.board[m.source()] = pieces_with_color::none;

		if( kingside ) {
			// Kingside

			// Target squares
			p.bitboards[p.self()][bb_type::all_pieces] ^= 0x60ull << row;
			p.bitboards[p.self()][bb_type::king] ^= 0x40ull << row;
			p.bitboards[p.self()][bb_type::rooks] ^= 0x20ull << row;

			delta += pst(p.self(), pieces::rook, row + 5) - pst(p.self(), pieces::rook, row + rook);
			std::swap(p.board[row + 5], p.board[row + rook]);

			p.hash_ ^= get_piece_hash( pieces::rook, p.self(), row + 5 );
			p.hash_ ^= get_piece_hash( pieces::rook, p.self(), row + rook );
		}
		else {
			// Queenside

			// Target squares
			p.bitboards[p.self()][bb_type::all_pieces] ^= 0x0cull << row;
			p.bitboards[p.self()][bb_type::king] ^= 0x04ull << row;
			p.bitboards[p.self()][bb_type::rooks] ^= 0x08ull << row;

			delta += pst(p.self(), pieces::rook, row + 3) - pst(p.self(), pieces::rook, row + rook);
			std::swap(p.board[row + 3], p.board[row + rook]);

			p.hash_ ^= get_piece_hash( pieces::rook, p.self(), row + 3 );
			p.hash_ ^= get_piece_hash( pieces::rook, p.self(), row + rook );
		}
		p.can_en_passant = 0;
		p.castle[p.self()] = 0;

		if( p.white() ) {
			p.base_eval += delta;
		}
		else {
			p.base_eval -= delta;
		}

		p.c = p.other();

		p.board[m.target()] = pwc; 

#if VERIFY_POSITION
		p.verify_abort();
#endif
		return;
	}

	p.bitboards[p.self()][piece] ^= source_square;
	p.bitboards[p.self()][bb_type::all_pieces] ^= source_square;

	if( captured_piece != pieces::none ) {
		if( m.enpassant() ) {
			unsigned char ep = (m.target() & 0x7) | (m.source() & 0x38);
			uint64_t const ep_square = 1ull << ep;
			p.bitboards[p.other()][bb_type::pawns] ^= ep_square;
			p.bitboards[p.other()][bb_type::all_pieces] ^= ep_square;
			p.board[ep] = pieces_with_color::none;
			p.hash_ ^= get_piece_hash( pieces::pawn, p.other(), ep );
		}
		else {
			p.hash_ ^= get_piece_hash( static_cast<pieces::type>(captured_piece), p.other(), m.target() );
			p.bitboards[p.other()][captured_piece] ^= target_square;
			p.bitboards[p.other()][bb_type::all_pieces] ^= target_square;

			if( captured_piece == pieces::rook ) {
				if( m.target() / 8 == (p.other() ? 7 : 0) && (1ull << (m.target() % 8)) & p.castle[p.other()] ) {
					p.hash_ ^= zobrist::castle[p.other()][1ull << (m.target() % 8)];
					p.castle[p.other()] ^= 1ull << (m.target() % 8);
				}
			}
		}

		if( captured_piece == pieces::pawn ) {
			uint64_t pawns = p.bitboards[p.other()][bb_type::pawns];
			if( p.white() ) {
				p.bitboards[p.other()][bb_type::pawn_control] =
					((pawns & 0xfefefefefefefefeull) >> 9) |
					((pawns & 0x7f7f7f7f7f7f7f7full) >> 7);
			}
			else {
				p.bitboards[p.other()][bb_type::pawn_control] =
					((pawns & 0xfefefefefefefefeull) << 7) |
					((pawns & 0x7f7f7f7f7f7f7f7full) << 9);
			}

			if( m.enpassant() ) {
				unsigned char ep = (m.target() & 0x7) | (m.source() & 0x38);
				p.pawn_hash ^= get_piece_hash( pieces::pawn, p.other(), ep );
				delta += pst(p.other(), pieces::pawn, ep) + eval_values::material_values[ pieces::pawn ];
			}
			else {
				p.pawn_hash ^= get_piece_hash( pieces::pawn, p.other(), m.target() );
				delta += pst(p.other(), pieces::pawn, m.target()) + eval_values::material_values[ pieces::pawn ];
			}
		}
		else {
			delta += pst(p.other(), captured_piece, m.target()) + eval_values::material_values[ captured_piece ];
			p.material[p.other()] -= eval_values::material_values[ captured_piece ];
		}

		p.piece_sum -= 1ull << ((captured_piece - 1 + (p.white() ? 5 : 0) ) * 6);
	}

	if( piece == pieces::rook ) {
		if( m.source() / 8 == (p.self() ? 7 : 0) && (1ull << (m.source() % 8)) & p.castle[p.self()] ) {
			p.hash_ ^= zobrist::castle[p.self()][1ull << (m.source() % 8)];
			p.castle[p.self()] ^= 1ull << (m.source() % 8);
		}
	}
	else if( piece == pieces::king ) {
		p.hash_ ^= zobrist::castle[p.self()][p.castle[p.self()]];
		p.castle[p.self()] = 0;
		p.king_pos[p.self()] = static_cast<square::type>(m.target());
	}

	if( piece == pieces::pawn && (m.source() ^ m.target()) == 16) {
		unsigned char ep_square = (m.target() / 8 + m.source() / 8) * 4 + m.target() % 8;
		if( (1ull << ep_square) & p.bitboards[p.other()][bb_type::pawn_control] ) {
			p.can_en_passant = ep_square;
			p.hash_ ^= zobrist::enpassant[ep_square];
		}
		else {
			p.can_en_passant = 0;
		}
	}
	else {
		p.can_en_passant = 0;
	}

	if( m.promotion() ) {
		pieces::type promotion_piece = m.promotion_piece();

		p.bitboards[p.self()][promotion_piece] ^= target_square;

		p.material[p.self()] += eval_values::material_values[ promotion_piece ];

		delta -= eval_values::material_values[pieces::pawn];
		delta += eval_values::material_values[promotion_piece] + pst(p.self(), promotion_piece, m.target());

		p.board[m.target()] = static_cast<pieces_with_color::type>(m.promotion_piece() + (p.white() ? 0 : 8));

		p.piece_sum -= 1ull << (p.black() ? 30 : 0);
		p.piece_sum += 1ull << ((promotion_piece - 1 + (p.black() ? 5 : 0) ) * 6);

		p.hash_ ^= get_piece_hash( promotion_piece, p.self(), m.target() );
	}
	else {
		p.bitboards[p.self()][piece] ^= target_square;
		delta += pst(p.self(), piece, m.target());
		p.board[m.target()] = pwc;
		p.hash_ ^= get_piece_hash( piece, p.self(), m.target() );
	}
	p.bitboards[p.self()][bb_type::all_pieces] ^= target_square;
	p.board[m.source()] = pieces_with_color::none;

	if( piece == pieces::pawn ) {
		uint64_t pawns = p.bitboards[p.self()][bb_type::pawns];
		if( p.white() ) {
			p.bitboards[p.self()][bb_type::pawn_control] =
				((pawns & 0xfefefefefefefefeull) << 7) |
				((pawns & 0x7f7f7f7f7f7f7f7full) << 9);
		}
		else {
			p.bitboards[p.self()][bb_type::pawn_control] =
				((pawns & 0xfefefefefefefefeull) >> 9) |
				((pawns & 0x7f7f7f7f7f7f7f7full) >> 7);
		}

		p.pawn_hash ^= get_piece_hash( pieces::pawn, p.self(), m.source());
		if( !m.promotion() ) {
			p.pawn_hash ^= get_piece_hash( pieces::pawn, p.self(), m.target() );
		}
	}

	if( p.white() ) {
		p.base_eval += delta;
	}
	else {
		p.base_eval -= delta;
	}

	p.c = p.other();

#if VERIFY_POSITION
	p.verify_abort();
#endif
}


void position::init_pawn_hash()
{
	pawn_hash = 0;
	for( int c = 0; c < 2; ++c ) {
		uint64_t cpawns = bitboards[c][bb_type::pawns];
		while( cpawns ) {
			uint64_t pawn = bitscan_unset( cpawns );
			pawn_hash ^= get_piece_hash( pieces::pawn, static_cast<color::type>(c), static_cast<unsigned char>(pawn) );
		}
	}
}


bool position::is_occupied_square( uint64_t square ) const {
	return get_occupancy( 1ull << square ) != 0;
}


uint64_t position::get_occupancy( uint64_t mask ) const
{
	return (bitboards[color::white][bb_type::all_pieces] | bitboards[color::black][bb_type::all_pieces]) & mask;
}


namespace {
static std::string board_square_to_string( position const& p, int pi )
{
	pieces_with_color::type pwc = p.get_piece_with_color( pi );
	pieces::type piece = get_piece( pwc );
	color::type c = get_color( pwc );

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


std::string row_to_string( position const& p, int row, color::type view )
{
	std::string ret;

	ret += '1' + row;
	ret += "| ";
	if( view == color::white ) {
		for( int col = 0; col < 8; ++col ) {
			ret += board_square_to_string( p, row * 8 + col );
			ret += ' ';
		}
	}
	else {
		for( int col = 7; col >= 0; --col ) {
			ret += board_square_to_string( p, row * 8 + col );
			ret += ' ';
		}
	}
	ret += "|\n";

	return ret;
}
}


std::string board_to_string( position const& p, color::type view )
{
	std::string ret;

	ret += " +-----------------+\n";
	if( view == color::white ) {
		for( int row = 7; row >= 0; --row ) {
			ret += row_to_string( p, row, view );
		}
	}
	else {
		for( int row = 0; row <= 7; ++row ) {
			ret += row_to_string( p, row, view );
		}
	}
	ret += " +-----------------+\n";
	if( view == color::white ) {
		ret += "   A B C D E F G H \n";
	}
	else {
		ret += "   H G F E D C B A \n";
	}

	return ret;
}

bool do_is_valid_move( position const& p, move const& m, check_map const& check )
{
	// Check we're moving own piece
	pieces_with_color::type pwc = p.get_piece_with_color( m );
	if( pwc == pieces_with_color::none || get_color(pwc) != p.self() ) {
		return false;
	}

	// Target must either be free or enemy piece
	pieces_with_color::type captured_pwc = p.get_captured_piece_with_color( m );

	pieces::type piece = get_piece(pwc);
	pieces::type captured_piece = get_piece(captured_pwc);

	if( piece == pieces::king ) {

		uint64_t other_kings = p.bitboards[p.other()][bb_type::king];
		uint64_t other_king = bitscan( other_kings );
		if( (1ull << m.target()) & possible_king_moves[other_king] ) {
			return false;
		}

		if( m.castle() ) {
			if( check.check ) {
				return false;
			}

			// Check source and target legality
			if( m.source() / 8 != (p.self() ? 7 : 0) ) {
				return false;
			}

			bool kingside;
			if( m.target() == (p.self() ? 62 : 6) ) {
				kingside = true;
			}
			else if( m.target() == (p.self() ? 58 : 2) ) {
				kingside = false;
			}
			else {
				return false;
			}

			if( !p.castle[p.c] ) {
				return false;
			}

			uint64_t row = p.self() ? 56 : 0;
			uint64_t rook = kingside ? bitscan_reverse(p.castle[p.c]) : bitscan(p.castle[p.c]);
			uint64_t rook_target = row + (kingside ? 5 : 3);

			if( kingside != (row + rook > m.source()) ) {
				return false;
			}
			
			// Check castling rights
			if( !(p.castle[p.self()] & (1ull << rook)) ) {
				return false;
			}

			// Check rook exists
			ASSERT(p.bitboards[p.self()][bb_type::rooks] & (1ull << (rook + row)));

			// Check not checked target square

			uint64_t all = p.get_occupancy();
			all &= ~(1ull << (row + rook));
			all &= ~(1ull << m.source());

			uint64_t king_between = between_squares[m.source()][m.target()];
			uint64_t rook_between = between_squares[row + rook][rook_target];

			if( all & (king_between | (1ull << m.target()) ) )  {
				return false;
			}
			if( all & (rook_between | (1ull << rook_target) ) ) {
				return false;
			}

			while( king_between ) {
				uint64_t sq = bitscan_unset(king_between);
				
				if( detect_check(p, p.self(), sq, row + rook) ) {
					return false;
				}
			}
			if( possible_king_moves[(m.source() + m.target()) / 2] & p.bitboards[p.other()][bb_type::king] ) {
				return false;
			}

			if( detect_check(p, p.self(), m.target(), row + rook) ) {
				return false;
			}
		}
		else {
			if( captured_pwc != pieces_with_color::none && get_color(captured_pwc) == p.self() ) {
				return false;
			}

			if( m.enpassant() || m.promotion() ) {
				return false;
			}
			else if( !((1ull << m.target()) & possible_king_moves[m.source()]) ) {
				return false;
			}
		}

		if( detect_check( p, p.self(), m.target(), m.source() ) ) {
			return false;
		}
	}
	else {
		if( captured_pwc != pieces_with_color::none && get_color(captured_pwc) == p.self() ) {
			return false;
		}

		if( check.check && check.multiple() ) {
			return false;
		}

		if( m.enpassant() ) {
			if( piece != pieces::pawn || captured_piece != pieces::pawn ) {
				return false;
			}

			if( p.can_en_passant != m.target() ) {
				return false;
			}

			if( p.get_piece_with_color( m.target() % 8 + (m.source() & 0xf8) ) != (p.white() ? pieces_with_color::black_pawn : pieces_with_color::white_pawn) ) {
				return false;
			}

			unsigned char new_col = m.target() % 8;
			unsigned char old_col = m.source() % 8;
			unsigned char old_row = m.source() / 8;

			unsigned char const& cv_old = check.board[m.source()];
			unsigned char const& cv_new = check.board[m.target()];
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

			// Special case: Captured pawn uncovers bishop/queen check
			// While this cannot occur in a regular game, board might have been setup like this.
			uint64_t occ = (p.bitboards[p.self()][bb_type::all_pieces] | p.bitboards[p.other()][bb_type::all_pieces]);
			occ &= ~(1ull << (new_col + old_row * 8));
			if( bishop_magic( p.king_pos[p.self()], occ) & (p.bitboards[p.other()][bb_type::bishops] | p.bitboards[p.other()][bb_type::queens] ) ) {
				return false;
			}

			// Special case: Enpassant capture uncovers a check against own king, e.g. in this position:
			// black queen, black pawn, white pawn, white king from left to right on rank 5
			unsigned char king_col = static_cast<unsigned char>(p.king_pos[p.self()] % 8);
			unsigned char king_row = static_cast<unsigned char>(p.king_pos[p.self()] / 8);

			if( king_row == old_row ) {
				uint64_t mask;
				if( king_col < old_col ) {
					mask = 0xff & ~((2 << king_col) - 1);
				}
				else {
					mask = (1 << king_col) - 1;
				}
				mask &= ~(1ull << old_col);
				mask &= ~(1ull << new_col);
				mask <<= 8 * king_row;

				uint64_t occ = (p.bitboards[p.self()][bb_type::all_pieces] | p.bitboards[p.other()][bb_type::all_pieces]) & mask;
				if( occ ) {
					uint64_t sq;
					if( king_col < old_col ) {
						sq = bitscan( occ );
					}
					else {
						sq = bitscan_reverse( occ );
					}

					pieces_with_color::type pwc = p.get_piece_with_color( sq );
					if( get_color(pwc) == p.other() ) {
						pieces::type pi = get_piece(pwc);
						if( pi == pieces::rook || pi == pieces::queen ) {
							return false;
						}
					}
				}
			}
		}
		else if( m.castle() ) {
			return false;
		}
		else {

			unsigned char const& cv_old = check.board[m.source()];
			unsigned char const& cv_new = check.board[m.target()];
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

			if( piece == pieces::pawn ) {
				bool would_promote = !(m.target() / 8) || (m.target()) / 8 == 7;
				if( would_promote != m.promotion() ) {
					return false;
				}
				if( (pawn_control[p.self()][m.source()] & 1ull << m.target()) ) {
					if( !captured_piece ) {
						return false;
					}
				}
				else {
					if( captured_piece ) {
						return false;
					}
					if( (m.source() ^ m.target()) & 0x7 ) {
						return false;
					}

					unsigned char pushed = m.source() + (p.white() ? 8 : -8);
					if( m.target() != pushed ) {
						if( m.source() / 8 != (p.white() ? 1 : 6) || m.target() / 8 != (p.white() ? 3 : 4) ) {
							return false;
						}

						if( p.get_piece(pushed) ) {
							return false;
						}
					}
				}
			}
			else if( m.promotion() ) {
				return false;
			}

			if( piece == pieces::knight ) {
				if( !(possible_knight_moves[m.source()] & (1ull << m.target()) ) ) {
					return false;
				}
			}
			else if( piece == pieces::bishop ) {
				uint64_t const all_blockers = p.bitboards[p.self()][bb_type::all_pieces] | p.bitboards[p.other()][bb_type::all_pieces];
				uint64_t possible_moves = bishop_magic( m.source(), all_blockers );
				if( !(possible_moves & (1ull << m.target() ) ) ) {
					return false;
				}
			}
			else if( piece == pieces::rook ) {
				uint64_t const all_blockers = p.bitboards[p.self()][bb_type::all_pieces] | p.bitboards[p.other()][bb_type::all_pieces];
				uint64_t possible_moves = rook_magic( m.source(), all_blockers );
				if( !(possible_moves & (1ull << m.target() ) ) ) {
					return false;
				}
			}
			else if( piece == pieces::queen) {
				uint64_t const all_blockers = p.bitboards[p.self()][bb_type::all_pieces] | p.bitboards[p.other()][bb_type::all_pieces];
				uint64_t possible_moves = rook_magic( m.source(), all_blockers ) | bishop_magic( m.source(), all_blockers );
				if( !(possible_moves & (1ull << m.target() ) ) ) {
					return false;
				}
			}
		}
	}

	return true;
}

bool is_valid_move( position const& p, move const& m, check_map const& check )
{
	bool ret = do_is_valid_move( p, m, check );

#if VERIFY_IS_VALID_MOVE
	move_info moves[200];
	move_info* it = moves;
	move_info* end = moves;
	calculate_moves<movegen_type::all>( p, end, check );
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
		std::cerr << board_to_string( p, color::white ) << std::endl;
		config tmp;
		tmp.fischer_random = true;
		std::cerr << position_to_fen_noclock( tmp, p ) << std::endl;
		std::cerr << move_to_string( p, m ) << std::endl;
		std::cerr << "Ret: " << ret << std::endl;
		abort();
	}
#endif

	return ret;
}
