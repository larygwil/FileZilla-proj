#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>

#include "util/platform.hpp"
#include "util/time.hpp"

// Upper limits, cannot go higher without recompilation.
#define MAX_DEPTH  60
#define MAX_QDEPTH 40

struct config
{
	config();

	// Returns the command to execute
	std::string init( int argc,  char const* argv[] );

	unsigned int thread_count;
	unsigned int memory;
	unsigned int max_moves; // only for auto play
	signed short depth;
	unsigned char quiescence_depth;

	duration time_limit;

	int random_seed;

	std::string logfile;

	bool ponder;

	bool use_book;
	std::string book_dir;

	int pawn_hash_table_size; // In MiB

	std::string program_name() const;

private:
	void init_book_dir( std::string self );
};
extern config conf;

#endif
