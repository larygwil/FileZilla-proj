#ifndef __ASSERT_H__
#define __ASSERT_H__

//#include <assert.h>
//#define ASSERT(x) assert(x);


#define ASSERT(x) do{ \
	if( !(x) ) { \
	std::cerr << "Assertion failed (" << __FILE__ << ":" << __LINE__ << "): " << #x << std::endl; \
	for(;;){}\
		abort(); \
	} \
	break; \
} while( true );
#undef ASSERT
#ifndef ASSERT
#define ASSERT(x)
#endif

#endif
