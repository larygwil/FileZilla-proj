#ifndef __BOOK_H__
#define __BOOK_H__

#include "chess.hpp"

#include <string>
#include <vector>

// Indexes get mapped to offsets where a book entry is stored. followed by list of move_entries until m.other is set

// Root position has index 0.
struct book_entry
{
	short reached_at_depth;
	short full_depth;
	short quiescence_depth;
};

struct move_entry {
	move m;
	short forecast;
	unsigned long long next_index;
} __attribute__((__packed__));

book_entry get_entries( unsigned long long index, std::vector<move_entry>& moves );

bool open_book( std::string const& book_dir );

bool needs_init();

unsigned long long book_add_entry( book_entry const& b, std::vector<std::pair<short, move> > const& moves );
void book_update_move( unsigned long long index, int move_index, unsigned long long new_index );

#endif
