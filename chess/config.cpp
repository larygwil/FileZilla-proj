#include "config.hpp"
#include "util/platform.hpp"

#include <iostream>
#include <algorithm>

#include <string.h>

config::config()
: thread_count(get_cpu_count()),
  memory(get_system_memory() / 3 ),
  max_moves(0),
  quiescence_depth(MAX_QDEPTH),
  time_limit( duration::hours(1) ),
  random_seed(-1), //-1 == based on time
  ponder(),
  use_book(true),
  depth_(-1),
  pawn_hash_table_size_(0)
{
	if( sizeof(void*) < 8 ) {
		// Limit default to 1GB on 32bit compile
		if( memory > 1024 ) {
			memory = 1024;
		}
	}
}


void config::init_self_dir( std::string self )
{
#if WINDOWS
	std::replace( self.begin(), self.end(), '\\', '/' );
#endif
	if( self.rfind('/') != std::string::npos ) {
		self_dir = self.substr( 0, self.rfind('/') + 1 );
	}
#if _MSC_VER
	if( GetFileAttributesA( (self_dir + "opening_book.db").c_str() ) == INVALID_FILE_ATTRIBUTES ) {
		char buffer[MAX_PATH];
		GetModuleFileNameA( 0, buffer, MAX_PATH );
		buffer[MAX_PATH - 1] = 0;
		self_dir = buffer;
		std::replace( self_dir.begin(), self_dir.end(), '\\', '/' );
		if( self_dir.rfind('/') != std::string::npos ) {
			self_dir = self_dir.substr( 0, self_dir.rfind('/') + 1 );
		}
	}
#endif
}


std::string config::init( int argc,  char const* argv[] )
{
	init_self_dir( argv[0] );

	int i = 1;
	for( i = 1; i < argc && argv[i][0] == '-'; ++i ) {
		std::string const opt = argv[i];
		if( opt == "--moves" ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << opt << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < 0 ) {
				std::cerr << "Invalid argument to " << opt << std::endl;
				exit(1);
			}
			conf.max_moves = v;
		}
		else if( opt == "--threads" ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << opt << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < 1 ) {
				std::cerr << "Invalid argument to " << opt << std::endl;
				exit(1);
			}
			conf.thread_count = v;
		}
		else if( opt == "--depth" ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << opt << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < 1 || v > MAX_DEPTH ) {
				std::cerr << "Invalid argument to " << opt << std::endl;
				exit(1);
			}
			conf.set_max_search_depth( v );
		}
		else if( opt == "--quiescence" ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << opt << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < 1 || v > MAX_QDEPTH ) {
				std::cerr << "Invalid argument to " << opt << std::endl;
				exit(1);
			}
			conf.quiescence_depth = v;
		}
		else if( opt == "--memory" ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << opt << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < 1 ) {
				std::cerr << "Invalid argument to " << opt << std::endl;
				exit(1);
			}
			conf.memory = v;
		}
		else if( opt == "--seed" ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << opt << std::endl;
				exit(1);
			}
			int v = atoi(argv[i]);
			if( v < -1 ) {
				std::cerr << "Invalid argument to " << opt << std::endl;
				exit(1);
			}
			conf.random_seed = v;
		}
		else if( opt == "--logfile" ) {
			if( ++i >= argc ) {
				std::cerr << "Missing argument to " << opt << std::endl;
				exit(1);
			}
			conf.logfile = argv[i];
		}
		else if( opt == "--ponder" ) {
			conf.ponder = true;
		}
		else if( opt == "--nobook" ) {
			conf.use_book = false;
		}
		else {
			std::cerr << "Unknown argument " << argv[i] << std::endl;
			exit(1);
		}
	}

	if( i < argc - 1 ) {
		std::cerr << "Command needs to be passed last on command-line." << std::endl;
		exit(1);
	}

	std::string cmd;
	if( i < argc ) {
		cmd = argv[i];
	}

	return cmd;
}

std::string config::program_name() const
{
	std::string name = "Octochess";
#ifdef REVISION
	name += " revision " REVISION;
#endif

	if( sizeof(void*) < 8 ) {
		name += " 32-bit";
	}

	return name;
}


int config::max_search_depth() const
{
	if( depth_ == -1 ) {
		return MAX_DEPTH;
	}
	else {
		return depth_;
	}
}


void config::set_max_search_depth( int depth )
{
	if( depth < -1 ) {
		depth_ = -1;
	}
	else if( depth > MAX_DEPTH ) {
		depth_ = -1;
	}
	else {
		depth_ = depth;
	}
}


unsigned int config::pawn_hash_table_size() const
{
	unsigned int ret = pawn_hash_table_size_;
	if( !ret ) {
		ret = std::min( 64, get_system_memory() / 8 );
	}

	return ret;
}

config conf;

