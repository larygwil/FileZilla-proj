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

std::vector<std::string> tokenize( std::string const& str )
{
	std::istringstream in( str );
	in.flags( std::stringstream::skipws );

	std::istream_iterator<std::string> it( in );
	std::istream_iterator<std::string> end;

	return std::vector<std::string>( it, end );
}
