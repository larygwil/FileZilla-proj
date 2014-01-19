#ifndef __ASSERT_H__
#define __ASSERT_H__

#include <iostream>

#define USE_ASSERT 0
#define VERIFY_POSITION 0
#define VERIFY_EVAL 0
#define VERIFY_IS_VALID_MOVE 0
#define CHECK_TYPE_1_COLLISION 0
#define VERIFY_FEN 0
#define VERIFY_PAWN_HASH_TABLE 0 // Only works single-threaded
#define VERIFY_KILLERS 0

#if USE_ASSERT
#define ASSERT(x) do{ \
	if( !(x) ) { \
		std::cerr << "Assertion failed (" << __FILE__ << ":" << __LINE__ << "): " << #x << std::endl; \
		::abort(); \
	} \
	break; \
} while( true );
#else
#define ASSERT(x)
#endif

#endif