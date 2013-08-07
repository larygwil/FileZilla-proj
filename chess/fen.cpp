#include "config.hpp"
#include "fen.hpp"
#include "util.hpp"
#include "tables.hpp"

#include <iostream>
#include <sstream>

std::string position_to_fen_noclock( config const& conf, position const& p )
{
	std::stringstream ss;

	for( int row = 7; row >= 0; --row ) {
		int free = 0;

		for( int col = 0; col < 8; ++col ) {
			int pi = row * 8 + col;

			pieces_with_color::type pwc = p.get_piece_with_color( pi );
			pieces::type piece = get_piece( pwc );
			color::type c = get_color( pwc );

			if( piece == pieces::none ) {
				++free;
				continue;
			}

			if( free ) {
				ss << free;
				free = 0;
			}

			unsigned char name;

			switch( piece ) {
			default:
			case pieces::king:
				name = 'K';
				break;
			case pieces::queen:
				name = 'Q';
				break;
			case pieces::rook:
				name = 'R';
				break;
			case pieces::bishop:
				name = 'B';
				break;
			case pieces::knight:
				name = 'N';
				break;
			case pieces::pawn:
				name = 'P';
				break;
			}

			if( c == color::black ) {
				name += 'a' - 'A';
			}

			ss << name;
		}

		if( free ) {
			ss << free;
		}

		if( row ) {
			ss << '/';
		}
	}

	if( p.white() ) {
		ss << " w ";
	}
	else {
		ss << " b ";
	}

	bool can_castle = false;
	if( !conf.fischer_random ) {
		if( p.castle[color::white] & 0x80 ) {
			can_castle = true;
			ss << 'K';
		}
		if( p.castle[color::white] & 0x01 ) {
			can_castle = true;
			ss << 'Q';
		}
		if( p.castle[color::black] & 0x80 ) {
			can_castle = true;
			ss << 'k';
		}
		if( p.castle[color::black] & 0x01 ) {
			can_castle = true;
			ss << 'q';
		}
	}
	else {
		for( unsigned int c = 0; c < 2; ++c ) {
			uint64_t castles = p.castle[c];
			while( castles ) {
				can_castle = true;
				uint64_t castle = bitscan_unset(castles);
				ss << static_cast<char>((c ? 'a' : 'A') + castle)	;
			}
		}
	}

	if( !can_castle ) {
		ss << '-';
	}

	ss << ' ';

	if( p.can_en_passant ) {
		ss << static_cast<unsigned char>('a' + (p.can_en_passant % 8));
		ss << static_cast<unsigned char>('1' + (p.can_en_passant / 8));
	}
	else {
		ss << '-';
	}

#if VERIFY_FEN
	position p2;
	if( !parse_fen( conf, ss.str(), p2 ) ) {
		std::cerr << "FAIL" << std::endl;
	}
#endif
	return ss.str();
}

bool parse_fen( config const& conf, std::string const& fen, position& p, std::string* error )
{
	std::stringstream in( fen );

	std::string board;
	std::string color;
	std::string castle;
	std::string enpassant;

	in >> board >> color >> castle >> enpassant;

	if( !in ) {
		if( error ) {
			*error = "String too short to be a legal FEN.";
		}
		return false;
	}

	if( color != "w" && color != "b" && color != "W" && color != "B" ) {
		if( error ) {
			*error = "Invalid side-to-move.";
		}
		return false;
	}
	if( color == "w" || color == "W" ) {
		p.c = color::white;
	}
	else {
		p.c = color::black;
	}

	p.castle[0] = 0;
	p.castle[1] = 0;

	p.can_en_passant = 0;

	if( enpassant != "-" ) {
		if( enpassant.size() != 2 ) {
			if( error ) {
				*error = "Invalid en-passant square.";
			}
			return false;
		}
		if( enpassant[0] >= 'A' && enpassant[0] <= 'H' ) {
			enpassant[0] = enpassant[0] + 'a' - 'A';
		}
		if( enpassant[0] < 'a' || enpassant[0] > 'h' || (enpassant[1] != '3' && enpassant[1] != '6') ) {
			if( error ) {
				*error = "Invalid en-passant square.";
			}
			return false;
		}

		p.can_en_passant = (enpassant[1] - '1') * 8 + enpassant[0] - 'a';
	}

	p.clear_bitboards();

	int row = 7;
	int col = 0;
	bool last_was_free = false;
	const char* cur = board.c_str();
	while( *cur ) {
		if( row < 0 ) {
			if( error ) {
				*error = "More than eight rows given.";
			}
			return false;
		}

		if( *cur == '/' ) {
			if( col != 8 ) {
				if( error ) {
					*error = "Less than eight squares specified in a row.";
				}
				return false;
			}
			--row;
			col = 0;
			++cur;
			last_was_free = false;
			continue;
		}

		if( col >= 8 ) {
			if( error ) {
				*error = "More than eight squares specified in a row.";
			}
			return false;
		}

		if( *cur >= '1' && *cur <= '8') {
			if( last_was_free ) {
				if( error ) {
					*error = "Two concecutive numbers denoting free squares in same row.";
				}
				return false;
			}
			col += *cur - '0';
			last_was_free = true;
		}
		else {
			last_was_free = false;

			unsigned char pi = col + row * 8;

			pieces::type piece;
			color::type c;
			switch( *cur ) {
			default:
				if( error ) {
					*error = "Invalid character in a row.";
				}
				return false;
			case 'p':
				c = color::black;
				piece = pieces::pawn;
				break;
			case 'n':
				c = color::black;
				piece = pieces::knight;
				break;
			case 'b':
				c = color::black;
				piece = pieces::bishop;
				break;
			case 'r':
				c = color::black;
				piece = pieces::rook;
				break;
			case 'q':
				c = color::black;
				piece = pieces::queen;
				break;
			case 'k':
				c = color::black;
				piece = pieces::king;
				break;
			case 'P':
				c = color::white;
				piece = pieces::pawn;
				break;
			case 'N':
				c = color::white;
				piece = pieces::knight;
				break;
			case 'B':
				c = color::white;
				piece = pieces::bishop;
				break;
			case 'R':
				c = color::white;
				piece = pieces::rook;
				break;
			case 'Q':
				c = color::white;
				piece = pieces::queen;
				break;
			case 'K':
				c = color::white;
				piece = pieces::king;
				break;
			}
			p.bitboards[c][piece] |= 1ull << pi;
			++col;
		}


		++cur;
	}

	p.update_derived();

	if( castle != "-" ) {
		for( std::size_t i = 0; i < castle.size(); ++i ) {
			switch( castle[i] ) {
			case 'Q':
				p.castle[0] |= 1ull << bitscan((p.bitboards[color::white][bb_type::rooks] & 0xffull) | (1ull << p.king_pos[0]));
				break;
			case 'K':
				p.castle[0] |= 1ull << bitscan_reverse((p.bitboards[color::white][bb_type::rooks] & 0xffull) | (1ull << p.king_pos[0]));
				break;
			case 'q':
				p.castle[1] |= 1ull << (bitscan((p.bitboards[color::black][bb_type::rooks] & 0xff00000000000000ull) | (1ull << p.king_pos[1])) - 56);
				break;
			case 'k':
				p.castle[1] |= 1ull << (bitscan_reverse((p.bitboards[color::black][bb_type::rooks] & 0xff00000000000000ull) | (1ull << p.king_pos[1])) - 56);
				break;
			case 'a':
			case 'b':
			case 'c':
			case 'd':
			case 'e':
			case 'f':
			case 'g':
			case 'h':
				p.castle[1] |= 1ull << (castle[i] - 'a');
				break;
			case 'A':
			case 'B':
			case 'C':
			case 'D':
			case 'E':
			case 'F':
			case 'G':
			case 'H':
				p.castle[0] |= 1ull << (castle[i] - 'A');
				break;
			default:
				if( error ) {
					*error = "Invalid castling rights.";
				}
				return false;
			}
		}
		p.hash_ = p.init_hash();
	}

	std::string tmp;
	if( !p.verify(tmp) || (!conf.fischer_random && !p.verify_castling( tmp, conf.fischer_random ) ) ) {
		if( error ) {
			*error = tmp;
		}
		return false;
	}

	unsigned int halfmoves_since_pawnmove_or_capture = 0;
	if( in >> halfmoves_since_pawnmove_or_capture ) {
		p.halfmoves_since_pawnmove_or_capture = halfmoves_since_pawnmove_or_capture;
	}

	return true;
}
