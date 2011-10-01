#include "book.hpp"
#include "util.hpp"

#include "sqlite/sqlite3.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace {
unsigned char const table[64] = {
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
  'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
  'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
  'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F',
  'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
  'W', 'X', 'Y', 'Z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', ',', '.' };


std::string move_to_book_string( move const& m )
{
	std::string ret;
	ret += table[m.source];
	ret += table[m.target];
	return ret;
}


std::string history_to_string( std::vector<move> const& history )
{
	std::string ret;
	for( std::vector<move>::const_iterator it = history.begin(); it != history.end(); ++it ) {
		ret += move_to_book_string(*it);
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

unsigned char conv_to_index( unsigned char s )
{
	if( s >= 'a' && s <= 'z' ) {
		return s - 'a';
	}
	else if( s >= 'A' && s <= 'Z' ) {
		return s - 'A' + 26;
	}
	else if( s >= '0' && s <= '9' ) {
		return s - '0' + 26 + 26;
	}
	else if( s == ',' ) {
		return 62;
	}
	else {//if( s == '.' ) {
		return 63;
	}
}

extern "C" int get_cb( void* p, int, char** data, char** /*names*/ ) {
	cb_data* d = reinterpret_cast<cb_data*>(p);

	unsigned char si = conv_to_index( data[0][0] );
	unsigned char ti = conv_to_index( data[0][1] );

	char ms[5] = {0};
	ms[0] = (si % 8) + 'a';
	ms[1] = (si / 8) + '1';
	ms[2] = (ti % 8) + 'a';
	ms[3] = (ti / 8) + '1';

	move m;
	if( !parse_move( d->p, d->c, ms, m ) ) {
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


std::vector<book_entry> book::get_entries( position const& p, color::type c, std::vector<move> const& history )
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


bool book::add_entries( std::vector<move> const& history, std::vector<book_entry> entries )
{
	std::sort( entries.begin(), entries.end() );

	std::string h = history_to_string( history );

	scoped_lock l(impl_->mtx);

	std::stringstream ss;
	ss << "BEGIN TRANSACTION;";
	for( std::vector<book_entry>::const_iterator it = entries.begin(); it != entries.end(); ++it ) {
		std::string m = move_to_book_string( it->m );
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
extern "C" int count_cb( void* p, int, char** data, char** /*names*/ ) {
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


void book::mark_for_processing( std::vector<move> history )
{
	std::stringstream ss;
	ss << "BEGIN TRANSACTION;";
	while( !history.empty() ) {
		std::string h = history_to_string( history );
		ss << "INSERT OR IGNORE INTO position (pos) VALUES ('" << h << "');";
		history.pop_back();
	}

	ss << "COMMIT TRANSACTION;";

	std::string query = ss.str();

	int res = sqlite3_exec( impl_->db, query.c_str(), 0, 0, 0 );

	if( res != SQLITE_OK ) {
		std::cerr << "Database failure" << std::endl;
		abort();
	}
}
