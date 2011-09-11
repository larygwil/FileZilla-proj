#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <string>

// Upper limits, cannot go higher without recompilation.
#define MAX_DEPTH  40
#define MAX_QDEPTH 40

struct config
{
	config();

	int init( int argc,  char const* argv[] );

	int thread_count;
	unsigned int memory;
	unsigned int max_moves; // only for auto play
	unsigned char depth;
	unsigned char quiescence_depth;

	unsigned long long time_limit; // In milliseconds

	int random_seed;

	std::string logfile;
};
extern config conf;

#endif
