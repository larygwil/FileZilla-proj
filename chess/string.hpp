#ifndef __STRING_H__
#define __STRING_H__

#include <vector>
#include <string>
#include <sstream>

std::string split( std::string const& str, std::string& args, std::string::value_type c = ' ' );


template< typename T >
inline std::string to_string( T const& t )
{
	std::stringstream ss;
	ss << t;
	return ss.str();
}


#endif
