#ifndef __BOOK_H__
#define __BOOK_H__

#include "calc.hpp"
#include "chess.hpp"

#include <list>
#include <string>
#include <vector>

struct book_entry
{
	move m;
	short forecast;
	short search_depth;

	bool operator<( book_entry const& rhs ) const {
		return forecast > rhs.forecast;
	}
};


struct work {
	std::vector<move> move_history;
	seen_positions seen;
	position p;
	color::type c;
};


class book
{
public:
	book( std::string const& book_dir );
	~book();

	bool is_open() const;

	// Returned entries are sorted by forecast, highest first.
	std::vector<book_entry> get_entries( position const& p, color::type c, std::vector<move> const& history );

	// Entries do not have to be sorted
	bool add_entries( std::vector<move> const& history, std::vector<book_entry> entries );

	void mark_for_processing( std::vector<move> history );

	unsigned long long size();

	std::list<work> get_unprocessed_positions();

private:
	class impl;
	impl *impl_;
};

#endif
