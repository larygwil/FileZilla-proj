#ifndef OCTOCHESS_UCI_TIME_CALCULATION_HEADER
#define OCTOCHESS_UCI_TIME_CALCULATION_HEADER

#include "types.hpp"
#include "../util/time.hpp"

namespace octochess {
namespace uci {

class position_time {
public:
	position_time()
		: moves_to_go_()
	{
		left_[0] = left_[1] = duration::infinity();
	}

	void set_time( bool c, duration const& t ) { left_[c] = t; }
	void set_increment( bool c, duration const& t ) { inc_[c] = t; }
	void set_movetime( duration const& t ) { movetime_ = t; }
	void set_moves_to_go( uint64_t const& moves_to_go ) { moves_to_go_ = moves_to_go; }

	duration time_left( bool c ) const { return left_[c]; }
	duration increment( bool c ) const { return inc_[c]; }
	duration movetime() const { return movetime_; }
	uint64_t moves_to_go() const { return moves_to_go_; }

private:
	duration left_[2];
	duration inc_[2];
	duration movetime_;
	uint64_t moves_to_go_;
};

class time_calculation {
public:
	time_calculation();

	void set_infinite_time();
	void update(position_time const&, bool is_black, int half_moves);
	void after_move_update( duration const& elapsed_time, duration const& used_extra_time );

	duration time_for_this_move() const { return time_limit_; }

	// Without overhead
	duration total_remaining() const { return time_remaining_; }

	// Per-move overhead
	duration overhead() const { return internal_overhead_; }
private:
	duration time_limit_;
	duration time_remaining_;
	duration bonus_time_;
	duration internal_overhead_;
};

}
}

#endif

