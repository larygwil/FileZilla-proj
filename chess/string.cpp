#include "string.hpp"

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
