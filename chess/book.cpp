#include "book.hpp"
#include "util.hpp"

#include "sqlite/sqlite3.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace {
std::string history_to_string( std::vector<std::string> const& history )
{
	std::string ret;
	for( std::vector<std::string>::const_iterator it = history.begin(); it != history.end(); ++it ) {
		ret += *it;
	}

	return ret;
}
}

class book::impl
{
public:
	mutex mtx;
	sqlite3* db;
};

book::book( std::string const& book_dir )
	: impl_( new impl )
{
	std::string fn( book_dir + "/opening_book.db" );
	sqlite3_open( fn.c_str(), &impl_->db );
}


book::~book()
{
	delete impl_;
}


bool book::is_open() const
{
	scoped_lock l(impl_->mtx);

	return impl_->db != 0;
}


namespace {
struct cb_data {
	std::vector<book_entry>* entries;
	position p;
	color::type c;
};

extern "C" int get_cb( void* p, int, char** data, char** names ) {
	cb_data* d = reinterpret_cast<cb_data*>(p);

	move m;
	if( !parse_move( d->p, d->c, data[0], m ) ) {
		return 1;
	}

	int forecast = atoi(data[1]);
	if( forecast > 32767 || forecast < -32768 ) {
		return 1;
	}
	int searchdepth = atoi(data[2]);
	if( searchdepth < 1 || searchdepth > 40 ) {
		return 1;
	}

	book_entry entry;
	entry.forecast = static_cast<short>(forecast);
	entry.search_depth = static_cast<short>(searchdepth);
	entry.m = m;
	d->entries->push_back( entry );

	return 0;
}
}


std::vector<book_entry> book::get_entries( position const& p, color::type c, std::vector<std::string> const& history )
{
	std::vector<book_entry> ret;

	scoped_lock l(impl_->mtx);

	if( !impl_->db ) {
		return ret;
	}

	cb_data data;
	data.p = p;
	data.c = c;
	data.entries = &ret;

	std::string h = history_to_string( history );

	std::string query = "SELECT move, forecast, searchdepth FROM book WHERE position = (SELECT id FROM position WHERE pos ='" + h + "') ORDER BY forecast DESC,searchdepth DESC";

	int res = sqlite3_exec( impl_->db, query.c_str(), &get_cb, reinterpret_cast<void*>(&data), 0 );

	if( res != SQLITE_DONE && res != SQLITE_OK ) {
		std::cerr << "Database failure" << std::endl;
		abort();
	}

	return ret;
}


bool book::add_entries( std::vector<std::string> const& history, std::vector<book_entry> entries )
{
	std::sort( entries.begin(), entries.end() );

	std::string h = history_to_string( history );

	scoped_lock l(impl_->mtx);

	std::stringstream ss;
	ss << "BEGIN TRANSACTION;";
	for( std::vector<book_entry>::const_iterator it = entries.begin(); it != entries.end(); ++it ) {
		std::string m = move_to_source_target_string( it->m );
		ss << "INSERT OR IGNORE INTO position (pos) VALUES ('" << h << "');";
		ss << "INSERT OR REPLACE INTO book (position, move, forecast, searchdepth) VALUES ((SELECT id FROM position WHERE pos='" << h << "'), '" << m << "', " << it->forecast << ", " << it->search_depth << ");";
	}
	ss << "COMMIT TRANSACTION;";

	std::string query = ss.str();

	int res = sqlite3_exec( impl_->db, query.c_str(), 0, 0, 0 );

	if( res != SQLITE_OK ) {
		std::cerr << "Database failure" << std::endl;
		abort();
	}
	return res == SQLITE_OK;
}


namespace {
extern "C" int count_cb( void* p, int, char** data, char** names ) {
	unsigned long long* count = reinterpret_cast<unsigned long long*>(p);
	*count = atoll( *data );

	return 0;
}
}


unsigned long long book::size()
{
	scoped_lock l(impl_->mtx);

	std::string query = "SELECT COUNT(id) FROM position;";

	unsigned long long count = 0;
	sqlite3_exec( impl_->db, query.c_str(), &count_cb, reinterpret_cast<void*>(&count), 0 );

	return count;
}
