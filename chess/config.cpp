#include "config.hpp"
#include "platform.hpp"

#include <iostream>

#include <string.h>

config::config()
: thread_count(get_cpu_count()),
  memory(get_system_memory() / 3 ),
  max_moves(0),
  depth(-1),
  quiescence_depth(MAX_QDEPTH),
  time_limit(3600*1000), // In ms
  random_seed(-1), //-1 == based on time
  ponder(),
  use_book(true)
{
	if( sizeof(void*) < 8 ) {
		// Limit default to 1GB on 32bit compile
		if( memory > 0x100000ull ) {
			memory = 0x100000ull;
		}
	}
}

int config::init( int argc,  char const* argv[] )
{
	int i = 1;
	for( i = 1; i < argc && argv[i][0] == '-'; ++i ) {
		if( !strcmp(argv[i], "--moves" ) ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << argv[i] << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < 0 ) {
				std::cerr << "Invalid argument to " << argv[i] << std::endl;
				exit(1);
			}
			conf.max_moves = v;
		}
		else if( !strcmp(argv[i], "--threads" ) ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << argv[i] << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < 1 ) {
				std::cerr << "Invalid argument to " << argv[i] << std::endl;
				exit(1);
			}
			conf.thread_count = v;
		}
		else if( !strcmp(argv[i], "--depth" ) ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << argv[i] << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < 1 || v > MAX_DEPTH ) {
				std::cerr << "Invalid argument to " << argv[i] << std::endl;
				exit(1);
			}
			conf.depth = v;
		}
		else if( !strcmp(argv[i], "--quiescence" ) ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << argv[i] << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < 1 || v > MAX_QDEPTH ) {
				std::cerr << "Invalid argument to " << argv[i] << std::endl;
				exit(1);
			}
			conf.quiescence_depth = v;
		}
		else if( !strcmp(argv[i], "--memory" ) ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << argv[i] << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < 1 ) {
				std::cerr << "Invalid argument to " << argv[i] << std::endl;
				exit(1);
			}
			conf.memory = v;
		}
		else if( !strcmp(argv[i], "--seed" ) ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << argv[i] << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < -1 ) {
				std::cerr << "Invalid argument to " << argv[i] << std::endl;
				exit(1);
			}
			conf.random_seed = v;
		}
		else if( !strcmp(argv[i], "--logfile" ) ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << argv[i] << std::endl;
				exit(1);
			}
			conf.logfile = argv[i];
		}
		else if( !strcmp(argv[i], "--ponder" ) ) {
			conf.ponder = true;
		}
		else if( !strcmp(argv[i], "--nobook" ) ) {
			conf.use_book = false;
		}
		else {
			std::cerr << "Unknown argument " << argv[i] << std::endl;
			exit(1);
		}
	}
	return i;
}

config conf;

