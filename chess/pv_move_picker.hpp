#ifndef __PV_MOVE_PICKER__
#define __PV_MOVE_PICKER__

#include "pvlist.hpp"

// If kept updated during a search, this class can be used to
// quickly make a move the next time the go command can be used
// in order to save some time.
// A situation in which this is used are recaptures.
class pv_move_picker
{
public:
	pv_move_picker();

	// First element is the move to execute, the second element a hint to ponder on.
	std::pair<move, move> can_use_move_from_pv( position const& p );

	void update_pv( position p, pv_entry const* pv );

private:
	uint64_t hash_;
	position previous_pos_;
	move previous_;
	position next_pos_;
	move next_;
	move ponder_;
};

#endif
