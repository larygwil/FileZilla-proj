#ifndef __CHESS_H__
#define __CHESS_H__

#include <string.h>

#include "util/platform.hpp"
#include "score.hpp"
#include "position.hpp"

PACKED(class move,
{
public:
	move()
		: u()
	{}

	bool empty() const { return piece == pieces::none; }

	union {
		uint64_t u;
		PACKED(struct,
		{
			unsigned char flags;
			pieces::type piece;
			unsigned char source;
			unsigned char target;
			pieces::type captured_piece;
		});
	};

	bool operator!=( move const& rhs ) const {
		return u != rhs.u;
	}
	bool operator==( move const& rhs ) const {
		return u == rhs.u;
	}
});

#endif
