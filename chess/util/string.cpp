#include "string.hpp"

#include <iterator>

std::string split( std::string const& str, std::string& args, std::string::value_type c )
{
	std::string ret;

	std::size_t pos = str.find( c );
	if( pos != std::string::npos ) {
		ret = str.substr( 0, pos );
		args = str.substr( pos + 1 );
	}
	else {
		ret = str;
		args.clear();
	}

	return ret;
}

std::vector<std::string> tokenize( std::string const& str, std::string const& sep, std::string::value_type quote )
{
	std::vector<std::string> ret;
	
	std::size_t pos = 0;
	std::size_t prev = 0;
	std::size_t q = str.find( quote );
	while( (pos = str.find_first_of( sep, prev )) != str.npos ) {
		if( pos != prev ) {

			if( q != str.npos && q < pos ) {
				q = str.find( quote, q + 1 );
				pos = str.find_first_of( sep, q + 1 );
				q = str.find( quote, q + 1 );
				if( pos == str.npos ) {
					break;
				}
			}
			std::string tok = str.substr( prev, pos - prev ); 
			if( quote && tok[0] == quote && tok[tok.size() - 1] == quote ) {
				tok = tok.substr( 1, tok.size() - 2 );
			}
			ret.push_back( tok );
		}
		prev = pos + 1;
	}
	if( prev < str.size() ) {
		std::string tok = str.substr( prev ); 
		if( quote && tok[0] == quote && tok[tok.size() - 1] == quote ) {
			tok = tok.substr( 1, tok.size() - 2 );
		}
		ret.push_back( str.substr( prev ) );
	}
	return ret;
}

void trim( std::string& str, std::string::value_type v )
{
	std::size_t begin = str.find_first_not_of(v);
	std::size_t end = str.find_last_not_of(v) + 1;
	if( begin == str.npos ) {
		// Empty string or all v
		return;
	}

	str = str.substr( begin, end - begin );
}