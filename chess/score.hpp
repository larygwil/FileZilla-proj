#ifndef __SCORE_H__
#define __SCORE_H__

/*
 * The score object represents evaluation scores. It can be used for partial evaluations
 * as well as complete positional evaluations.
 *
 * The score is being represented by two shorts, one for middle-game and one for end-game
 * evaluation. This distinction is necessary as the evaluation terms are a lot different
 * between game phases.
 *
 * For the search it becomes necessary to extract a single value from the composite score.
 * This value is scaled between the middle-game and end-game score based on the material
 * still on the board.
 */

#include <ostream>

class score
{
public:
	typedef short value_type;

	score();
	score( value_type mg, value_type eg );
	score( score const& rhs );

	short scale( value_type material ) const;

	score operator+( score const& rhs ) const;
	score operator-( score const& rhs ) const;

	score operator-() const;

	score& operator+=( score const& rhs );
	score& operator-=( score const& rhs );

	score& operator=( score const& rhs );

	value_type mg() const { return mg_; }
	value_type eg() const { return eg_; }

	value_type& mg() { return mg_; }
	value_type& eg() { return eg_; }

	typedef value_type&(score::*phase_ref)();

	bool operator==( score const& rhs ) const;
	bool operator!=( score const& rhs ) const;

	score operator*( value_type m ) const;
	score operator*( score const& m ) const;

	score& operator*=( value_type m );

	score operator/( value_type m ) const;
	score operator/( score const& m ) const;

private:
	value_type mg_;
	value_type eg_;
};

std::ostream& operator<<(std::ostream&, score const&);

#endif
