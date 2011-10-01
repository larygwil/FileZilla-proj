#ifndef __BOOK_H__
#define __BOOK_H__

#include "chess.hpp"

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

	unsigned long long size();

private:
	class impl;
	impl *impl_;
};

#endif
