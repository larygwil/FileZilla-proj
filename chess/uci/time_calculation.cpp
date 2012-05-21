
#include "time_calculation.hpp"

#include "../chess.hpp"

#include <algorithm>

namespace octochess {
namespace uci {

time_calculation::time_calculation() 
{
}

void time_calculation::set_infinite_time() {
	time_limit_ = duration::infinity();
	time_remaining_ = duration::infinity();
}

void time_calculation::update(position_time const& t, bool is_white, int half_moves) {

	if( t.movetime().empty() ) {
		uint64_t remaining_moves = std::max( 20, (80 - half_moves) / 2 );

		duration inc;
		if( is_white ) {
			time_remaining_ = t.white_time_left();
			inc = t.white_increment();
		}
		else {
			time_remaining_ = t.black_time_left();
			inc = t.black_increment();
		}

		time_limit_ = (time_remaining_ - bonus_time_) / remaining_moves + bonus_time_;

		if( !inc.empty() && time_remaining_ > (time_limit_ + inc) ) {
			time_limit_ += inc;
		}
		}
	else {
		time_limit_ = t.movetime();
	}

	// Any less time makes no sense.
	if( time_limit_ < duration::milliseconds(10) ) {
		time_limit_ = duration::milliseconds(10);
	}
}

void time_calculation::after_move_update( duration const& elapsed ) {
	if( time_limit_ > elapsed ) {
		bonus_time_ = (time_limit_ - elapsed) / 2;
	}
	else {
		bonus_time_ = duration();
	}
	time_remaining_ -= elapsed;
}

}
}

