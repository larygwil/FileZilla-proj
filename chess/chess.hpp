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
		: d()
	{}

	move( unsigned short source, unsigned short target, unsigned short flags ) 
	: d(source | (target << 6) | flags)
	{}
	
	// Getters
	bool empty() const { return d == 0; }

	unsigned char source() const { return d & 0x3f; }
	unsigned char target() const { return (d >> 6) & 0x3f; }

	bool castle() const { return (d & 0x3000) == 0x1000; }
	bool enpassant() const { return (d & 0x3000) == 0x2000; }
	bool promotion() const { return (d & 0x3000) == 0x3000; }

	pieces::type promotion_piece() const { return static_cast<pieces::type>((d >> 14) + 2); }

	// Setters
	void clear() { d = 0; }
	

	// Operators
	bool operator!=( move const& rhs ) const {
		return d != rhs.d;
	}
	bool operator==( move const& rhs ) const {
		return d == rhs.d;
	}

	bool operator<( move const& rhs ) const {
		return d < rhs.d;
	}

	// Data
	unsigned short d;
});

#endif
