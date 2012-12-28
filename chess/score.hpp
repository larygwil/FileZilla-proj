#ifndef __SCORE_H__
#define __SCORE_H__

/*
 * The score object represents evaluations score. It can be used for partial evaluations
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
	score();
	score( short mg, short eg );
	score( score const& rhs );

	short scale( short material ) const;

	score operator+( score const& rhs ) const;
	score operator-( score const& rhs ) const;

	score operator-() const;

	score& operator+=( score const& rhs );
	score& operator-=( score const& rhs );

	score& operator=( score const& rhs );

	short mg() const { return mg_; }
	short eg() const { return eg_; }

	short& mg() { return mg_; }
	short& eg() { return eg_; }

	bool operator==( score const& rhs ) const;
	bool operator!=( score const& rhs ) const;

	score operator*( short m ) const;
	score operator*( score const& m ) const;

	score operator/( short m ) const;
	score operator/( score const& m ) const;

private:
	short mg_;
	short eg_;
};

std::ostream& operator<<(std::ostream&, score const&);

#endif
