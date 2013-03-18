#ifndef OCTOCHESS_SIMPLE_BOOK_HEADER
#define OCTOCHESS_SIMPLE_BOOK_HEADER

#include "chess.hpp"

#include <string>
#include <vector>

extern unsigned short const simple_book_version;

struct simple_book_entry
{
	simple_book_entry()
		: forecast()
	{}

	move m;
	short forecast;
};

class simple_book
{
	class impl;
public:
	simple_book( std::string const& book_dir );
	virtual ~simple_book();

	bool open( std::string const& book_dir );
	void close();

	bool is_open() const;

	std::vector<simple_book_entry> get_entries( position const& p );

	impl* impl_;
};

#endif