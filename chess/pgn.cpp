#include "pgn.hpp"
#include "util.hpp"

#include <iostream>
#include <sstream>

pgn_reader::pgn_reader()
{
}


bool pgn_reader::open( std::string const& file )
{
	if( file.empty() ) {
		std::cerr << "No filename given" << std::endl;
	}

	in_.open( file );
	return in_.is_open();
}


bool pgn_reader::next_line( std::string& line ) {
	if( !pushback_.empty() ) {
		line = pushback_;
		pushback_.clear();
		return true;
	}

	return std::getline( in_, line );
}

bool pgn_reader::next( game& g )
{
	std::string line;


	bool valid = true;

	// Indicates whether we're in a braced comment.
	// Braced comments do not nest.
	bool in_brace = false;

	// Number of open Recursive Annotation Variation, started with '(', closed with ')'.
	unsigned int rav_stack = 0;

	color::type c = color::white;
	position p;
	init_board( p );
	g.p_ = p;
	g.c_ = c;
	g.moves_.clear();

	while( next_line( line ) ) {

		if( line.empty() ) {
			if( !g.moves_.empty() ) {
				return true;
			}
			continue;
		}

		if( line[0] == '[' ) {
			if( !g.moves_.empty() ) {
				pushback_ = line;
				return true;
			}
			valid = true;
			continue;
		}

		if( !valid ) {
			continue;
		}

		std::istringstream ss( line );
		std::string token;

		while( !token.empty() || ss >> token ) {
			if( !in_brace ) {
				if( token[0] == ';' ) {
					// Comment till end of line, ignore.
					break;
				}
				if( token[0] == '{' ) {
					in_brace = true;
					token = token.substr( 1 );
				}
			}

			if( in_brace ) {
				std::size_t pos = token.find('}');
				if( pos != std::string::npos ) {
					in_brace = false;
					token = token.substr( pos + 1 );
				}
				else {
					token.clear();
				}
				continue;
			}

			if( token[0] == '(' ) {
				++rav_stack;
				token = token.substr( 1 );
				continue;
			}

			if( rav_stack ) {
				// Unfortunately we currently cannot handle variations properly as our move parser cannot handle braces.
				std::size_t pos = token.find(')');
				if( pos != std::string::npos ) {
					--rav_stack;
					token = token.substr( pos + 1 );
				}
				else {
					token.clear();
				}
				continue;
			}

			if( token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "1/2" || token == "*" ) {
				valid = false;
				break;
			}

			if( token == "+" || token == "#" ) {
				token.clear();
				continue;
			}

			if( token[0] == '$' ) {
				if( token.size() < 2 && !isdigit(token[1]) ) {
					valid = false;
					std::cerr << "Invalid token: " << token;
					std::cerr << "Line: " << line << std::endl;
					break;
				}

				std::size_t i = 1;
				for( ; i < token.size() && isdigit(token[i]); ++i ) {
				}
				token = token.substr( i );
				continue;
			}

			if( isdigit(token[0]) ) {
				std::ostringstream os;
				os << (g.moves_.size() / 2) + 1;
				if( g.moves_.size() % 2 ) {
					os << "...";
				}
				else {
					os << ".";
				}

				if( token.length() < os.str().length() || token.substr( 0, os.str().length() ) != os.str() ) {
					valid = false;
					std::cerr << "Invalid move number token: " << token <<std::endl;
					std::cerr << "Expected: " << os.str() << std::endl;
					break;
				}

				token = token.substr( os.str().length() );

				if( token.empty() ) {
					continue;
				}
			}

			move m;
			if( !parse_move( p, c, token, m ) ) {
				std::cerr << "Invalid move: " << token << std::endl;
				std::cerr << "Line: " << line << std::endl;
				valid = false;
				break;
			}

			apply_move( p, m, c );
			c = static_cast<color::type>(1-c);
			g.moves_.push_back(m);
			token.clear();
		}
	}

	return !g.moves_.empty();
}
