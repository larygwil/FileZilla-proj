#include "util/platform.hpp"
#include "random.hpp"

#include <list>

random::random()
{
	seed(get_time());
}

random::random( uint64_t s )
{
	seed( s );
	seed(s);
}


void random::seed( uint64_t s )
{
	scoped_lock l( m_ ) ;
	engine_.seed( static_cast<unsigned long>(s) );
}


unsigned char random::get_unsigned_char()
{
	scoped_lock l( m_ ) ;
	return static_cast<unsigned char>(engine_());
}
		
uint64_t random::get_uint64()
{
	scoped_lock l( m_ ) ;
	return engine_();
}
