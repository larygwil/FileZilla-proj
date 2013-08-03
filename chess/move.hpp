#ifndef OCTOCHESS_MOVE_HEADER
#define OCTOCHESS_MOVE_HEADER

#include "definitions.hpp"

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
