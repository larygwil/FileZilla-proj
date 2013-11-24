#ifndef __HISTORY_H__
#define __HISTORY_H__

#include "moves.hpp"

class history
{
public:
	history();

	void clear();

	void reduce();

	void record( position const& p, move const& m );
	void record_cut( position const& p, move const& m, int processed );

	int get_value( pieces::type piece, move const& m, color::type c ) const;

private:
	uint64_t cut_[2][8][64];
	uint64_t all_[2][8][64];
};

#endif