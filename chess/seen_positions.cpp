#include "seen_positions.hpp"
#include "assert.hpp"

seen_positions::seen_positions( uint64_t root_hash )
	: pos()
	, root_position()
	, null_move_position()
{
	pos[0] = root_hash;
}


void seen_positions::clone_from( seen_positions const& s, int ply )
{
	ASSERT( this != &s );
	root_position = s.root_position;
	null_move_position = s.null_move_position;
	for( unsigned int i = null_move_position; i <= root_position + ply; ++i ) {
		pos[i] = s.pos[i];
	}
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
	for( int i = static_cast<int>(root_position) + ply - 4; i >= static_cast<int>(null_move_position); i -= 2 ) {
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


null_move_block::null_move_block( seen_positions& seen, uint64_t hash, int ply )
	: seen_(seen)
	, old_null_(seen.null_move_position)
{
	seen.pos[seen.root_position + ply] = hash;
	seen.null_move_position = seen.root_position + ply;
}


null_move_block::~null_move_block()
{
	seen_.null_move_position = old_null_;
}
