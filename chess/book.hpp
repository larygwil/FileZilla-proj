#ifndef __BOOK_H__
#define __BOOK_H__

#include "chess.hpp"

#include <string>
#include <vector>

// Indexes get mapped to offsets where a book entry is stored, followed by list of move_entries

// Root position has index 0.
struct book_entry
{
	unsigned char reached_at_depth;
	unsigned char count_moves;
} __attribute__((__packed__));

struct move_entry {
	unsigned short source_col : 3;
	unsigned short source_row : 3;
	unsigned short target_col : 3;
	unsigned short target_row : 3;
	unsigned short other : 2;

	unsigned char full_depth;
	unsigned char quiescence_depth;
	short forecast;

	unsigned long long next_index;

	void set_move( move const& m );
	move get_move() const;
	bool operator>=( move_entry const& rhs ) const;
} __attribute__((__packed__));

book_entry get_entries( unsigned long long index, std::vector<move_entry>& moves );

bool open_book( std::string const& book_dir );

bool needs_init();

unsigned long long book_add_entry( book_entry b, std::vector<move_entry> const& moves );
void book_update_move( unsigned long long index, int move_index, unsigned long long new_index );

#endif
