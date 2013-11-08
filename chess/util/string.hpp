#ifndef __STRING_H__
#define __STRING_H__

#include <vector>
#include <string>
#include <sstream>
#include <limits>

std::string split( std::string const& str, std::string& args, std::string::value_type c = ' ' );

std::vector<std::string> tokenize( std::string const& str, std::string const& sep = " ", std::string::value_type quote = 0 );

template< typename T >
inline std::string to_string( T const& t )
{
	std::stringstream ss;
	ss << t;
	return ss.str();
}


template<typename T>
bool to_int( std::string const& s, T& t, T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max(), bool clamp = true )
{
	bool ret = false;

	if( min <= max ) {
		std::stringstream ss;
		ss.flags(std::stringstream::skipws);
		ss.str(s);

		ss >> t;

		if( ss ) {
			if( t < min ) {
				t = min;
				ret = clamp;
			}
			else if( t > max ) {
				t = max;
				ret = clamp;
			}
			else {
				ret = true;
			}
		}
		else {
			t = min;
		}
	}
	else {
		// Can't do much here, at least don't leave t uninitizliazed.
		t = min;
	}

	return ret;
}

inline
bool to_bool( std::string const& s, bool& b )
{
	bool ret = false;
	if( s == "true" || s == "TRUE" || s == "True" || s == "1" || s == "yes" || s == "YES" || s == "Yes" ) {
		b = true;
		ret = true;
	}
	else if( s == "false" || s == "FALSE" || s == "False" || s == "0" || s == "no" || s == "NO" || s == "No" ) {
		b = false;
		ret = true;
	}

	return ret;
}


void trim( std::string& str, std::string::value_type v = ' ' );

#endif
