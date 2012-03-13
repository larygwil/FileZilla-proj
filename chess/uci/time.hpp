#ifndef OCTOCHESS_UCI_TIME_HEADER
#define OCTOCHESS_UCI_TIME_HEADER

#include "types.hpp"

namespace octochess {
namespace uci {

class position_time {
public:
	position_time() 
		: white_left_(), black_left_(), white_inc_(), black_inc_() 
	{}

	void set_white_time( time t ) { white_left_ = t; }
	void set_black_time( time t ) { black_left_ = t; }
	void set_white_increment( time t ) { white_inc_ = t; }
	void set_black_increment( time t ) { black_inc_ = t; }

	time white_time_left() const { return white_left_; }
	time black_time_left() const { return black_left_; }
	time white_increment() const { return white_inc_; }
	time black_increment() const { return black_inc_; }

private:
	time white_left_;
	time black_left_;
	time white_inc_;
	time black_inc_;
};

class time_calculation {
public:
	time_calculation();

	void set_infinite_time();
	void update(position_time const&, bool is_white, int half_moves);
	void after_move_update( uint elapsed_time );

	uint64_t time_for_this_move() const { return time_limit_; }
private:
	uint64_t time_limit_;
	uint64_t time_remaining_;
	uint64_t bonus_time_;
};

}
}

#endif

