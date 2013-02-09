#ifndef __LOGGER_H__
#define __LOGGER_H__

#include <iostream>
#include <ostream>

namespace logger {
	void init( std::string const& fn );
	void cleanup();

	void log_input( std::string const& in );

	void show_debug( bool show );
}

std::ostream& dlog();

#endif
