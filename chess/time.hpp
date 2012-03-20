#ifndef __TIME_H__
#define __TIME_H__

#include "platform.hpp"

#include <algorithm>

class duration;

class timestamp
{
public:
	timestamp();

	bool operator<( timestamp const& rhs ) const;

	timestamp operator+( duration const& rhs ) const;
private:
	friend class duration;
	int64_t t_;
};


class duration
{
public:
	duration();
	duration( timestamp const& start, timestamp const& stop );

	int64_t hours() const;
	int64_t minutes() const;
	int64_t seconds() const;
	int64_t milliseconds() const;
	int64_t nanoseconds() const;
	int64_t picoseconds() const;

	static duration hours( int64_t h );
	static duration minutes( int64_t m );
	static duration seconds( int64_t s );
	static duration milliseconds( int64_t ms );
	static duration nanoseconds( int64_t ns );
	static duration picoseconds( int64_t ns );

	bool negative() const;

	bool empty() const;

	duration absolute() const;

	duration operator-() const;
	
	duration operator+( duration const& rhs ) const;
	duration& operator+=( duration const& rhs );

	duration operator-( duration const& rhs ) const;
	duration& operator-=( duration const& rhs );

	duration operator*( int64_t mul ) const;
	duration& operator*=( int64_t mul );

	duration operator/( int64_t div ) const;
	duration& operator/=( int64_t div );

	bool operator!=( duration const& rhs ) const;

	bool operator<( duration const& rhs ) const;
	bool operator<=( duration const& rhs ) const;
	bool operator>( duration const& rhs ) const;
	bool operator>=( duration const& rhs ) const;

	int64_t get_items_per_second( int64_t count ) const;

	static duration infinity();
private:
	friend class timestamp;

	int64_t d_;
};

duration operator-( timestamp const& lhs, timestamp const& rhs );

namespace std {
inline duration abs( duration const& rhs ) {
	return rhs.absolute();
}
}

#endif
