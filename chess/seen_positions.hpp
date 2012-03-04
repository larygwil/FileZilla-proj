#ifndef __SEEN_POSITIONS_H__
#define __SEEN_POSITIONS_H__

#include "platform.hpp"
#include "config.hpp"

class seen_positions {
public:
	seen_positions( uint64_t root_hash = 0 );

	bool is_two_fold( uint64_t hash, int ply ) const;
	bool is_three_fold( uint64_t hash, int ply ) const;

	unsigned int depth() const { return root_position; };

	void set( uint64_t hash, int ply );
	void push_root( uint64_t hash );
	void pop_root( unsigned int count = 1 );
	void reset_root( uint64_t hash );

private:
	friend class null_move_block;

	uint64_t pos[100 + MAX_DEPTH + MAX_QDEPTH + 10]; // Must be at least 50 full moves + max depth and add some safety margin.
	unsigned int root_position; // Index of root position in seen_positions

	unsigned int null_move_position;
};

class null_move_block {
public:
	null_move_block( seen_positions& seen, int ply );
	~null_move_block();

private:
	seen_positions& seen_;
	unsigned int old_null_;
};

#endif
