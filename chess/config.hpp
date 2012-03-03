#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>

// Upper limits, cannot go higher without recompilation.
#define MAX_DEPTH  40
#define MAX_QDEPTH 40

struct config
{
	config();

	// Returns the command to execute
	std::string init( int argc,  char const* argv[] );

	int thread_count;
	unsigned int memory;
	unsigned int max_moves; // only for auto play
	signed char depth;
	unsigned char quiescence_depth;

	uint64_t time_limit; // In milliseconds

	int random_seed;

	std::string logfile;

	bool ponder;

	bool use_book;
	std::string book_dir;

	int pawn_hash_table_size; // In MiB

private:
	void init_book_dir( std::string const& self );
};
extern config conf;

#endif
