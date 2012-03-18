#include "assert.hpp"
#include "eval.hpp"
#include "score.hpp"


score::score()
	: mg_()
	, eg_()
{
}


score::score( short mg, short eg )
	: mg_(mg)
	, eg_(eg)
{
}


score::score( score const& rhs )
	: mg_(rhs.mg_)
	, eg_(rhs.eg_)
{
}


score& score::operator=( score const& rhs )
{
	mg_ = rhs.mg_;
	eg_ = rhs.eg_;

	return *this;
}


short score::scale( short material ) const
{
	if( material >= eval_values.phase_transition_material_begin ) {
		return mg_;
	}
	else if( material <= eval_values.phase_transition_material_end ) {
		return eg_;
	}
	
	int position = 256 * (eval_values.phase_transition_material_begin - material) / static_cast<int>(eval_values.phase_transition_duration);
	return ((static_cast<int>(mg_) * position      ) /256) +
		   ((static_cast<int>(eg_) * (256-position)) /256);
}


score score::operator+( score const& rhs ) const
{
	score ret = *this;
	ret += rhs;
	return ret;
}


score score::operator-( score const& rhs ) const
{
	score ret = *this;
	ret -= rhs;
	return ret;
}


score score::operator-() const
{
	return score( -mg_, -eg_ );
}


score& score::operator+=( score const& rhs )
{
	ASSERT( static_cast<int>(mg_) + rhs.mg_ < 32768 );
	ASSERT( static_cast<int>(mg_) + rhs.mg_ >= -32768 );
	ASSERT( static_cast<int>(eg_) + rhs.eg_ < 32768 );
	ASSERT( static_cast<int>(eg_) + rhs.eg_ >= -32768 );

	mg_ += rhs.mg_;
	eg_ += rhs.eg_;

	return *this;
}


score& score::operator-=( score const& rhs )
{
	ASSERT( static_cast<int>(mg_) - rhs.mg_ < 32768 );
	ASSERT( static_cast<int>(mg_) - rhs.mg_ >= -32768 );
	ASSERT( static_cast<int>(eg_) - rhs.eg_ < 32768 );
	ASSERT( static_cast<int>(eg_) - rhs.eg_ >= -32768 );

	mg_ -= rhs.mg_;
	eg_ -= rhs.eg_;

	return *this;
}


bool score::operator==( score const& rhs ) const {
	return mg_ == rhs.mg_ && eg_ == rhs.eg_;
}


bool score::operator!=( score const& rhs ) const {
	return !(*this == rhs);
}


std::ostream& operator<<(std::ostream& stream, score const& s)
{
	return stream << s.mg() << " " << s.eg();
}


score score::operator*( short m ) const
{
	ASSERT( static_cast<int>(mg_) * m < 32768 );
	ASSERT( static_cast<int>(mg_) * m >= -32768 );
	ASSERT( static_cast<int>(eg_) * m < 32768 );
	ASSERT( static_cast<int>(eg_) * m >= -32768 );

	return score( mg_ * m, eg_ * m );
}
