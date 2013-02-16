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

	duration time_limit;

	int random_seed;

	std::string logfile;

	bool ponder;

	bool use_book;
	std::string self_dir;

	std::string program_name() const;

	int max_search_depth() const;
	void set_max_search_depth( int depth );

	unsigned int pawn_hash_table_size() const;
private:
	void init_self_dir( std::string self );

	int depth_;
	unsigned int pawn_hash_table_size_; // In MiB
};
extern config conf;

#endif
