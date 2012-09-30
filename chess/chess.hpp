#ifndef __CHESS_H__
#define __CHESS_H__

#include <string.h>

#include "util/platform.hpp"
#include "score.hpp"
#include "position.hpp"

namespace square {
enum type {
	a1, b1, c1, d1, e1, f1, g1, h1,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a8, b8, c8, d8, e8, f8, g8, h8
};
}

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

	void clear() {
		u = 0;
	}

});

#endif
