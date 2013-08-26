#include "platform.hpp"

#if WINDOWS
#include "windows.cpp"
#else
#include "unix.cpp"
#endif

#include <locale>

namespace {
class platform_initializer
{
public:
	platform_initializer()
	{
		try {
			std::locale("");
		}
		catch( std::exception const& ) {
			// Ignore errors
		}
	};
};

static platform_initializer platint;
}
