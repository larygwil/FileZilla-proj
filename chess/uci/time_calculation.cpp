
#include "time_calculation.hpp"

#include "../chess.hpp"

#include <algorithm>
#include <iostream>

namespace octochess {
namespace uci {

time_calculation::time_calculation() 
: internal_overhead_( duration::milliseconds(50) )
{
}

void time_calculation::set_infinite_time() {
	time_limit_ = duration::infinity();
	time_remaining_ = duration::infinity();
}

void time_calculation::update(position_time const& t, bool is_white, int half_moves) {

	if( t.movetime().empty() ) {
		uint64_t remaining_moves = std::max( 20, (80 - half_moves) / 2 );
		if( t.moves_to_go() && t.moves_to_go() < remaining_moves ) {
			remaining_moves = t.moves_to_go();
		}

		duration inc;
		if( is_white ) {
			time_remaining_ = t.white_time_left();
			inc = t.white_increment();
		}
		else {
			time_remaining_ = t.black_time_left();
			inc = t.black_increment();
		}

		if( bonus_time_ > time_remaining_ ) {
			bonus_time_.clear();
		}

		time_limit_ = (time_remaining_ - bonus_time_) / remaining_moves + bonus_time_;

		if( !inc.empty() && time_remaining_ > (time_limit_ + inc) ) {
			time_limit_ += inc;
		}
	}
	else {
		time_limit_ = t.movetime();
	}

	duration overhead = internal_overhead_;
	if( time_limit_ > overhead ) {
		time_limit_ -= overhead;
	}
	else {
		time_limit_ = duration();
	}

	// Any less time makes no sense.
	if( time_limit_ < duration::milliseconds(10) ) {
		time_limit_ = duration::milliseconds(10);
	}
}

void time_calculation::after_move_update( duration const& elapsed, duration const& used_extra_time )
{
	if( time_limit_.is_infinity() ) {
		bonus_time_.clear();
	}
	else if( time_limit_ > elapsed ) {
		bonus_time_ = (time_limit_ - elapsed) / 2;
	}
	else {
		bonus_time_ = duration();

		if( time_limit_ + used_extra_time > elapsed ) {
			duration actual_overhead = elapsed - time_limit_ - used_extra_time;
			if( actual_overhead > internal_overhead_ ) {
				std::cerr << "Updating internal overhead from " << internal_overhead_.milliseconds() << " ms to " << actual_overhead.milliseconds() << " ms " << std::endl;
				internal_overhead_ = actual_overhead;
			}
		}
	}
	time_remaining_ -= elapsed;
}

}
}

