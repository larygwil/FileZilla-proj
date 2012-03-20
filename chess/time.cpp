#include "time.hpp"


namespace {
int64_t gcd( int64_t a, int64_t b )
{
	if( a < 0 ) {
		a = -a;
	}
	if( b < 0 ) {
		b = -b;
	}

	if( !a ) {
		return b;
	}

	while( b != 0 ) {
		if( a > b ) {
			a -= b;
		}
		else {
			b -= a;
		}
	}

	return a;
}

int64_t muldiv( int64_t v, int64_t mul, int64_t div )
{
	// Divide mul and div by its greatest common divisor.
	// This doesn't change the result per-se, but avoids 
	// integer overflows if either value is large.
	int64_t g = gcd( mul, div );
	mul /= g;
	div /= g;

	// Do the same with v and div
	g = gcd( v, div );
	v /= g;
	div /= g;

	return (v * mul) / div;
}
}


timestamp::timestamp()
: t_( get_time() )
{
}


bool timestamp::operator<( timestamp const& rhs ) const
{
	return t_ < rhs.t_;
}


timestamp timestamp::operator+( duration const& rhs ) const
{
	timestamp ret( *this );
	ret.t_ += rhs.d_;
	return ret;
}


duration::duration()
	: d_()
{
}


duration::duration( timestamp const& lhs, timestamp const& rhs )
	: d_( lhs.t_ - rhs.t_ )
{
}


int64_t duration::hours() const
{
	return d_ / timer_precision() / 3600;
}


duration duration::hours( int64_t h )
{
	duration ret;
	ret.d_ = h * timer_precision() * 3600;

	return ret;
}


int64_t duration::minutes() const
{
	return d_ / timer_precision() / 60;
}


duration duration::minutes( int64_t m )
{
	duration ret;
	ret.d_ = m * timer_precision() * 60;

	return ret;
}


int64_t duration::seconds() const
{
	return d_ / timer_precision();
}


duration duration::seconds( int64_t s )
{
	duration ret;
	ret.d_ = s * timer_precision();

	return ret;
}


int64_t duration::milliseconds() const
{
	return muldiv(d_, 1000, timer_precision());
}


duration duration::milliseconds( int64_t ms )
{
	duration ret;
	ret.d_ = muldiv(ms, timer_precision(), 1000);

	return ret;
}


int64_t duration::nanoseconds() const
{
	return muldiv(d_, 1000000000ll, timer_precision());
}


duration duration::nanoseconds( int64_t ns )
{
	duration ret;
	ret.d_ = muldiv(ns, timer_precision(), 1000000000ll);
	return ret;
}


int64_t duration::picoseconds() const
{
	return muldiv(d_, 1000000000000ll, timer_precision());
}


duration duration::picoseconds( int64_t ps )
{
	duration ret;
	ret.d_ = muldiv(ps, timer_precision(), 1000000000000ll);
	return ret;
}


duration duration::operator-() const
{
	duration ret;
	ret.d_ = -d_;

	return ret;
}


duration operator-( timestamp const& lhs, timestamp const& rhs )
{
	return duration( lhs, rhs );
}


duration duration::operator+( duration const& rhs ) const
{
	duration ret( *this );
	ret += rhs;

	return ret;
}


duration& duration::operator+=( duration const& rhs )
{
	d_ += rhs.d_;
	return *this;
}

duration duration::operator-( duration const& rhs ) const
{
	duration ret( *this );
	ret -= rhs;

	return ret;
}


duration& duration::operator-=( duration const& rhs )
{
	d_ -= rhs.d_;
	return *this;
}


duration duration::operator*( int64_t mul ) const
{
	duration ret( *this );
	ret *= mul;

	return ret;
}


duration& duration::operator*=( int64_t mul )
{
	d_ *= mul;
	return *this;
}


duration duration::operator/( int64_t div ) const
{
	duration ret( *this );
	ret /= div;

	return ret;
}


duration& duration::operator/=( int64_t div )
{
	d_ /= div;
	return *this;
}


bool duration::negative() const {
	return d_ < 0;
}


bool duration::empty() const
{
	return d_ == 0;
}


duration duration::absolute() const {
	if( negative() ) {
		return -*this;
	}
	else {
		return *this;
	}
}


bool duration::operator<( duration const& rhs ) const
{
	return d_ < rhs.d_;
}


bool duration::operator<=( duration const& rhs ) const
{
	return d_ <= rhs.d_;
}


bool duration::operator>( duration const& rhs ) const
{
	return d_ > rhs.d_;
}


bool duration::operator>=( duration const& rhs ) const
{
	return d_ >= rhs.d_;
}


int64_t duration::get_items_per_second( int64_t count ) const
{
	return muldiv( d_, timer_precision(), count );
}


duration duration::infinity()
{
	duration ret;
	ret.d_ = 0x7fffffffffffffffll;
	return ret;
}


bool duration::operator!=( duration const& rhs ) const
{
	return d_ != rhs.d_;
}
