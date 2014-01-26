#ifndef OCTOCHESS_TIME_CALCULATION_HEADER
#define OCTOCHESS_TIME_CALCULATION_HEADER

#include "util/time.hpp"

class time_calculation {
public:
	time_calculation();

	void reset( bool all = false );

	// Easy moves are recaptures for example
	std::pair<duration, duration> update( bool current_clock, unsigned int halfmoves_played, bool easy_move );

	void update_after_move( duration const& used_extra, bool add_increment,  bool keep_bonus );

	timestamp start() const { return start_; };
	void set_start( timestamp const& t );

	void set_movetime( duration const& t );

	// In full moves. If abs_to_go it's an absolute number, otherwise
	// number of moves since last time control.
	void set_moves_to_go( uint64_t to_go, bool abs_to_go );

	duration remaining( bool c ) const { return remaining_[c]; }
	void set_remaining( bool c, duration const& t, unsigned int halfmoves_played ) { remaining_[c] = t; remaining_set_at_[c] = halfmoves_played; }
	void set_increment( bool c, duration const& t ) { inc_[c] = t; }

	// If we receive time updates between moves, communication_overhead is the >=0 difference between two timer updates
	// and the calculated time consumption.
	duration communication_overhead_;

private:
	duration remaining_[2];
	unsigned int remaining_set_at_[2];
	duration inc_[2];
	bool current_clock_;

	// Time for next move
	duration limit_;

	// If we use less time than allocated for a move, we can use it on the next move
	duration bonus_;

	timestamp start_;
	duration movetime_;
	uint64_t moves_to_go_;
	bool abs_to_go_;

	// If we calculate move time of x but consume y > x amount of time, internal overhead if y - x.
	// This is measured locally between receiving the go and sending out the reply.
	duration internal_overhead_;
};

#endif

