#include "config.hpp"
#include "platform.hpp"

#include <iostream>
#include <algorithm>

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
  use_book(true),
  pawn_hash_table_size(100)
{
	if( sizeof(void*) < 8 ) {
		// Limit default to 1GB on 32bit compile
		if( memory > 0x100000ull ) {
			memory = 0x100000ull;
		}
	}
}


void config::init_book_dir( std::string self )
{
#if _MSC_VER
	std::replace( self.begin(), self.end(), '\\', '/' );
#endif
	if( self.rfind('/') != std::string::npos ) {
		book_dir = self.substr( 0, self.rfind('/') + 1 );
	}
#if _MSC_VER
	if( GetFileAttributesA( (book_dir + "/opening_book.db").c_str() ) == INVALID_FILE_ATTRIBUTES ) {
		char buffer[MAX_PATH];
		GetModuleFileNameA( 0, buffer, MAX_PATH );
		buffer[MAX_PATH - 1] = 0;
		book_dir = buffer;
		std::replace( book_dir.begin(), book_dir.end(), '\\', '/' );
		if( book_dir.rfind('/') != std::string::npos ) {
			book_dir = book_dir.substr( 0, book_dir.rfind('/') + 1 );
		}
	}
#endif
}


std::string config::init( int argc,  char const* argv[] )
{
	init_book_dir( argv[0] );

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
			conf.depth = v;
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

config conf;

