#include "util/platform.hpp"
#include "random.hpp"

#include <list>

randgen::randgen()
{
	seed(get_time());
}

randgen::randgen( uint64_t s )
{
	seed( s );
}


uint64_t randgen::seed( uint64_t s )
{
	scoped_lock l( m_ ) ;
	engine_.seed( static_cast<unsigned long>(s) );
	return s;
}


unsigned char randgen::get_unsigned_char()
{
	scoped_lock l( m_ ) ;
	return static_cast<unsigned char>(engine_());
}

uint64_t randgen::get_uint64()
{
	scoped_lock l( m_ ) ;
	return engine_();
}
