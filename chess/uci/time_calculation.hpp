#ifndef OCTOCHESS_UCI_TIME_CALCULATION_HEADER
#define OCTOCHESS_UCI_TIME_CALCULATION_HEADER

#include "types.hpp"
#include "../time.hpp"

namespace octochess {
namespace uci {

class position_time {
public:
	position_time() {}

	void set_white_time( duration t ) { white_left_ = t; }
	void set_black_time( duration t ) { black_left_ = t; }
	void set_white_increment( duration t ) { white_inc_ = t; }
	void set_black_increment( duration t ) { black_inc_ = t; }

	duration white_time_left() const { return white_left_; }
	duration black_time_left() const { return black_left_; }
	duration white_increment() const { return white_inc_; }
	duration black_increment() const { return black_inc_; }

private:
	duration white_left_;
	duration black_left_;
	duration white_inc_;
	duration black_inc_;
};

class time_calculation {
public:
	time_calculation();

	void set_infinite_time();
	void update(position_time const&, bool is_white, int half_moves);
	void after_move_update( duration const& elapsed_time );

	duration time_for_this_move() const { return time_limit_; }
	duration total_remaining() const { return time_remaining_; }
private:
	duration time_limit_;
	duration time_remaining_;
	duration bonus_time_;
};

}
}

#endif

