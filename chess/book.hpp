#ifndef __BOOK_H__
#define __BOOK_H__

#include "calc.hpp"
#include "chess.hpp"

#include <list>
#include <map>
#include <string>
#include <vector>

extern int const eval_version;

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


struct book_entry_with_position
{
	work w;
	std::vector<book_entry> entries;
};


struct stat_entry {
	stat_entry() : processed(), queued() {}

	int processed;
	int queued;
};

struct book_stats {
	book_stats() : total_processed(1), total_queued() {}

	std::map<int, stat_entry> data;

	int total_processed;
	int total_queued;
};

class book
{
public:
	book( std::string const& book_dir );
	~book();

	bool is_open() const;

	// Returned entries are sorted by forecast, highest first.
	// move_limit limits the number of returned moves per position, sorted descendingly by forecast.
	std::vector<book_entry> get_entries( position const& p, color::type c, std::vector<move> const& history, int move_limit = -1 );

	// Entries do not have to be sorted
	bool add_entries( std::vector<move> const& history, std::vector<book_entry> entries );

	void mark_for_processing( std::vector<move> history );

	unsigned long long size();

	book_stats stats();

	std::list<work> get_unprocessed_positions();

	// move_limit limits the number of returned moves per position, sorted descendingly by forecast.
	// max_evaluation_version ensures that only entries with an eval_version lower or equal than the given version are returned.
	std::list<book_entry_with_position> get_all_entries( int move_limit = -1 );

	bool update_entry( std::vector<move> const& history, book_entry const& entry );

	void fold();

private:
	class impl;
	impl *impl_;
};

#endif
