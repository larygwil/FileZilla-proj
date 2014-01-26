#include "time_calculation.hpp"
#include "util.hpp"

#include "util/logger.hpp"

time_calculation::time_calculation() 
: current_clock_(1)
, moves_to_go_()
, abs_to_go_()
, internal_overhead_( duration::milliseconds(50) )
{
	reset(true);
}

void time_calculation::reset(bool all)
{
	bonus_.clear();
	if( all ) {
		remaining_[0] = remaining_[1] = duration::infinity();
		inc_[0] = inc_[1] = duration();
		movetime_.clear();
		moves_to_go_ = 0;
		remaining_set_at_[0] = remaining_set_at_[1] = 9999999;
	}
}

std::pair<duration, duration> time_calculation::update( bool current_clock, unsigned int halfmoves_played, bool easy_move )
{
	current_clock_ = current_clock;
	duration elapsed = timestamp() - start_;
	if( elapsed > duration() ) {
		remaining_[current_clock] -= elapsed;
	}

	duration deadline;

	if( !movetime_.empty() ) {
		limit_ = duration::infinity();
		deadline = movetime_;
	}
	else {
		if( bonus_ > remaining_[current_clock] ) {
			bonus_ = duration();
		}

		uint64_t remaining_moves = std::max( 20, (82 - static_cast<int>(halfmoves_played)) / 2 );
		if( moves_to_go_ ) {
			uint64_t remaining;
			if( abs_to_go_ ) {
				remaining = moves_to_go_;
			}
			else {
				remaining = ((moves_to_go_ * 2) - (halfmoves_played % (moves_to_go_ * 2)) + 2) / 2;
			}

			if( remaining < remaining_moves ) {
				remaining_moves = remaining;
			}
		}

		limit_ = (remaining_[current_clock] - bonus_) / remaining_moves + bonus_;

		if( !inc_[current_clock].empty() && remaining_[current_clock] > (limit_ + inc_[current_clock]) ) {
			limit_ += inc_[current_clock];
		}

		deadline = remaining_[current_clock];

		// Easy moves are not worth to spend much time on
		if( easy_move ) {
			limit_ /= 2;
		}

		// If we've got more time to think than our opponent, use some extra time.
		if( remaining_set_at_[1 - current_clock] == halfmoves_played || remaining_set_at_[1 - current_clock] == halfmoves_played - 1 ) {
			if( remaining_[current_clock] > remaining_[1-current_clock] + limit_ * 2 ) {
				limit_ += limit_ / 4;
			}
		}
	}

	duration const overhead = internal_overhead_ + communication_overhead_;
	limit_ -= overhead;
	deadline -= overhead;

	// Any less time makes no sense.
	set_max( limit_, duration::milliseconds(10) );
	set_max( deadline, duration::milliseconds(10) );

	return std::make_pair( limit_, deadline );
}

void time_calculation::update_after_move( duration const& used_extra, bool add_increment,  bool keep_bonus )
{
	duration elapsed = timestamp() - start_;

	dlog() << "Elapsed: " << elapsed.milliseconds() << " ms" << std::endl;

	if( !keep_bonus || limit_.is_infinity() || elapsed < duration() ) {
		bonus_.clear();
	}
	else if( limit_ > elapsed ) {
		bonus_ = (limit_ - elapsed) / 2;
	}
	else {
		bonus_.clear();

		if( limit_ + used_extra < elapsed ) {
			duration actual_overhead = elapsed - limit_ - used_extra;
			if( actual_overhead > internal_overhead_ ) {
				dlog() << "Updating internal overhead from " << internal_overhead_.milliseconds() << " ms to " << actual_overhead.milliseconds() << " ms " << std::endl;
				internal_overhead_ = actual_overhead;
			}
		}
	}
	if( add_increment ) {
		remaining_[current_clock_] += inc_[current_clock_];
	}
	
	if( elapsed > duration() ) {
		remaining_[current_clock_] -= elapsed;
	}
}

void time_calculation::set_start( timestamp const& t )
{
	start_ = t;
}

void time_calculation::set_movetime( duration const& t )
{
	movetime_ = t;
}

void time_calculation::set_moves_to_go( uint64_t to_go, bool abs_to_go )
{
	moves_to_go_ = to_go;
	abs_to_go_ = abs_to_go;
}
