#include "config.hpp"

#include <iostream>

#include <string.h>

config::config()
: thread_count(6),
  memory(2048+1024),
  max_moves(0),
  depth(8),
  quiescence_depth(5),
  time_limit(3600*1000), // In ms
  random_seed(-1) //-1 == based on time
{}

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
			if( v < 1 || v > 40 ) {
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
			if( v < 1 || v > 20 ) {
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
		else {
			std::cerr << "Unknown argument " << argv[i] << std::endl;
			exit(1);
		}
	}
	return i;
}

config conf;

