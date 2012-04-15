#ifndef __STRING_H__
#define __STRING_H__

#include <vector>
#include <string>
#include <sstream>
#include <limits>

std::string split( std::string const& str, std::string& args, std::string::value_type c = ' ' );


template< typename T >
inline std::string to_string( T const& t )
{
	std::stringstream ss;
	ss << t;
	return ss.str();
}


template<typename T>
bool to_int( std::string const& s, T& t, T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max() )
{
	bool ret = false;

	std::stringstream ss;
	ss.flags(std::stringstream::skipws);
	ss.str(s);

	ss >> t;

	ret = ss;

	if( ss ) {
		if( t < min ) {
			t = min;
		}
		else if( t > max ) {
			t = max;
		}
	}

	return ret;
}

#endif
