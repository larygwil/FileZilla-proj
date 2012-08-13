#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <iostream>

namespace logger {
	void init( std::string const& fn );
	void cleanup();

	void log_input( std::string const& in );
}

#endif
