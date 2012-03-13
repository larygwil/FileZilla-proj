
#include "time.hpp"

#include "../chess.hpp"

#include <numeric>

namespace octochess {
namespace uci {

time_calculation::time_calculation() 
	: time_limit_()
	, time_remaining_()
	, bonus_time_()
{
}

void time_calculation::set_infinite_time() {
	time_limit_ = static_cast<uint64_t>(-1);
	time_remaining_ = static_cast<uint64_t>(-1);
}

void time_calculation::update(position_time const& t, bool is_white, int half_moves) {
	uint64_t remaining_moves = (std::max)( 20, (80 - half_moves) / 2 );

	time inc = 0;
	if( is_white ) {
		time_remaining_ = t.white_time_left();
		inc = (t.white_increment() * timer_precision()) / 1000;
	} else {
		time_remaining_ = t.black_time_left();
		inc = (t.black_increment() * timer_precision()) / 1000;
	}

	time_remaining_ = (time_remaining_ * timer_precision()) / 1000;
	time_limit_ = (time_remaining_ - bonus_time_) / remaining_moves + bonus_time_;

	if( inc && time_remaining_ > (time_limit_ + inc) ) {
		time_limit_ += inc;
	}
		
	// Any less time makes no sense.
	if( time_limit_ < 10 * timer_precision() / 1000 ) {
		time_limit_ = 10 * timer_precision() / 1000;
	}
}

void time_calculation::after_move_update( uint elapsed ) {
	if( time_limit_ > elapsed ) {
		bonus_time_ = (time_limit_ - elapsed) / 2;
	} else {
		bonus_time_ = 0;
	}
	time_remaining_ -= elapsed;
}

}
}

