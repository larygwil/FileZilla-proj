#ifndef __CONFIG_H__
#define __CONFIG_H__

struct config
{
	config();

	int init( int argc,  char const* argv[] );

	int thread_count;
	unsigned int memory;
	unsigned int max_moves; // only for auto play
	unsigned char depth;
	unsigned char quiescence_depth;
};
extern config conf;

#endif
