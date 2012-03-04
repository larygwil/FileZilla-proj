#include "seen_positions.hpp"

#include <string.h>

seen_positions::seen_positions( uint64_t root_hash )
	: root_position()
	, null_move_position()
{
	memset( pos, 0, (100 + MAX_DEPTH + MAX_QDEPTH + 10)*8 );
	pos[0] = root_hash;
}


bool seen_positions::is_three_fold( uint64_t hash, int ply ) const
{
	int count = 0;
	for( int i = root_position + ply - 4; i >= static_cast<int>(null_move_position); i -= 2) {
		if( pos[i] == hash ) {
			++count;
		}
	}
	return count >= 2;
}


bool seen_positions::is_two_fold( uint64_t hash, int ply ) const
{
	for( int i = root_position + ply - 4; i >= static_cast<int>(null_move_position); i -= 2 ) {
		if( pos[i] == hash ) {
			return true;
		}
	}
	return false;
}


void seen_positions::set( uint64_t hash, int ply )
{
	pos[root_position + ply] = hash;
}


void seen_positions::push_root( uint64_t hash )
{
	pos[++root_position] = hash;
}


void seen_positions::reset_root( uint64_t hash )
{
	root_position = 0;
	pos[0] = hash;
}


void seen_positions::pop_root( unsigned int count )
{
	if( count > root_position ) {
		root_position = 0;
	}
	else {
		root_position -= count;
	}
}


null_move_block::null_move_block( seen_positions& seen, int ply )
	: seen_(seen)
	, old_null_(seen.null_move_position)
{
	seen.null_move_position = seen.root_position + ply - 1;
}


null_move_block::~null_move_block()
{
	seen_.null_move_position = old_null_;
}
