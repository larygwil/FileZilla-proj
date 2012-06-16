#ifndef __HISTORY_H__
#define __HISTORY_H__

#include "moves.hpp"

class history
{
public:
	history();

	void clear();

	void reduce();

	// Last move is the one producing the cut
	void record_cut( move_info const* begin, move_info const* end, color::type c );

	int get_value( move const& m, color::type c ) const;

private:
	uint64_t cut_[2][7][64];
	uint64_t all_[2][7][64];
};

#endif