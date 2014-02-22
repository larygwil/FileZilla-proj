#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>

#include "util/platform.hpp"
#include "util/time.hpp"

// Upper limits, cannot go higher without recompilation.
#ifndef MAX_DEPTH 
#define MAX_DEPTH 75
#endif
#ifndef MAX_QDEPTH
#define MAX_QDEPTH 40
#endif
#ifndef DEPTH_FACTOR
#define DEPTH_FACTOR 6
#endif

class config
{
public:
	config();

	// Returns the command to execute
	std::string init( int argc, char const* argv[] );

	unsigned int thread_count;
	unsigned int memory;
	unsigned int max_moves; // only for auto play

	duration time_limit;

	std::string logfile;

	bool ponder;

	bool use_book;
	std::string self_dir;

	std::string program_name() const;

	int max_search_depth() const;
	void set_max_search_depth( int depth );

	unsigned int pawn_hash_table_size() const;

	bool fischer_random;
private:
	void init_self_dir( std::string self );

	int depth_;
};

#endif
