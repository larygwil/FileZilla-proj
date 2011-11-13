#include "book.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include "sqlite/sqlite3.h"

#include <algorithm>
#include <iostream>
#include <sstream>

int const eval_version = 3;

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
	impl() : db()
	{
	}

	mutex mtx;
	sqlite3* db;
};

book::book( std::string const& book_dir )
	: impl_( new impl )
{
	std::string fn( book_dir + "opening_book.db" );
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


bool conv_to_move( position const& p, color::type c, move& m, char const* data ) {
	unsigned char si = conv_to_index( data[0] );
	unsigned char ti = conv_to_index( data[1] );

	char ms[5] = {0};
	ms[0] = (si % 8) + 'a';
	ms[1] = (si / 8) + '1';
	ms[2] = (ti % 8) + 'a';
	ms[3] = (ti / 8) + '1';

	if( !parse_move( p, c, ms, m ) ) {
		return false;
	}

	return true;
}

extern "C" int get_cb( void* p, int, char** data, char** /*names*/ ) {
	cb_data* d = reinterpret_cast<cb_data*>(p);

	move m;
	if( !conv_to_move( d->p, d->c, m, data[0] ) ) {
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


std::vector<book_entry> book::get_entries( position const& p, color::type c, std::vector<move> const& history, int move_limit )
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

	std::stringstream ss;
	ss << "SELECT move, forecast, searchdepth FROM book WHERE position = (SELECT id FROM position WHERE pos ='" << h << "') ORDER BY forecast DESC,searchdepth DESC";

	if( move_limit != -1 ) {
		ss << " LIMIT " << move_limit;
	}

	std::string query = ss.str();
	int res = sqlite3_exec( impl_->db, query.c_str(), &get_cb, reinterpret_cast<void*>(&data), 0 );

	if( res != SQLITE_DONE && res != SQLITE_OK ) {
		std::cerr << "Database failure" << std::endl;
		std::cerr << "Failed query: " << query << std::endl;
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
		ss << "INSERT OR REPLACE INTO book (position, move, forecast, searchdepth, eval_version) VALUES ((SELECT id FROM position WHERE pos='" << h << "'), '"
		   << m << "', "
		   << it->forecast << ", "
		   << it->search_depth << ", "
		   << eval_version
		   << ");";
	}
	ss << "COMMIT TRANSACTION;";

	std::string query = ss.str();

	int res = sqlite3_exec( impl_->db, query.c_str(), 0, 0, 0 );

	if( res != SQLITE_OK ) {
		std::cerr << "Database failure" << std::endl;
		std::cerr << "Failed query: " << query << std::endl;
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
	scoped_lock l(impl_->mtx);

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
		std::cerr << "Failed query: " << query << std::endl;
		abort();
	}
}


namespace {
extern "C" int work_cb( void* p, int, char** data, char** /*names*/ )
{
	std::list<work>* wl = reinterpret_cast<std::list<work>*>(p);

	std::string pos = *data;
	if( pos.length() % 2 ) {
		return 1;
	}

	work w;
	init_board( w.p );
	w.c = color::white;
	w.seen.pos[0] = get_zobrist_hash(w.p);

	while( !pos.empty() ) {
		std::string ms = pos.substr( 0, 2 );
		pos = pos.substr( 2 );

		move m;
		if( !conv_to_move( w.p, w.c, m, ms.c_str() ) ) {
			return 1;
		}

		apply_move( w.p, m, w.c );
		w.c = static_cast<color::type>(1-w.c);
		w.move_history.push_back( m );
		w.seen.pos[++w.seen.root_position] = get_zobrist_hash(w.p);
	}

	wl->push_back( w );

	return 0;
}
}


std::list<work> book::get_unprocessed_positions()
{
	std::list<work> ret;

	scoped_lock l(impl_->mtx);

	std::string query = "SELECT pos FROM position WHERE id NOT IN (SELECT DISTINCT(position) FROM book) ORDER BY LENGTH(pos);";

	int res = sqlite3_exec( impl_->db, query.c_str(), &work_cb, reinterpret_cast<void*>(&ret), 0 );
	if( res != SQLITE_OK && res != SQLITE_DONE ) {
		std::cerr << "Database failure" << std::endl;
		std::cerr << "Failed query: " << query << std::endl;
		abort();
	}

	return ret;
}


std::list<book_entry_with_position> book::get_all_entries( int move_limit )
{
	std::list<book_entry_with_position> ret;

	std::list<work> positions;

	{
		scoped_lock l(impl_->mtx);

		std::string query = "SELECT pos FROM position ORDER BY LENGTH(pos)";

		int res = sqlite3_exec( impl_->db, query.c_str(), &work_cb, reinterpret_cast<void*>(&positions), 0 );
		if( res != SQLITE_OK && res != SQLITE_DONE ) {
			std::cerr << "Database failure" << std::endl;
			std::cerr << "Failed query: " << query << std::endl;
			abort();
		}
	}

	for( std::list<work>::const_iterator it = positions.begin(); it != positions.end(); ++it ) {

		book_entry_with_position entry;
		entry.w = *it;
		entry.entries = get_entries( it->p, it->c, it->move_history, move_limit );

		if( !entry.entries.empty() ) {
			ret.push_back( entry );
		}
	}

	return ret;
}

bool book::update_entry( std::vector<move> const& history, book_entry const& entry )
{
	std::string h = history_to_string( history );

	scoped_lock l(impl_->mtx);

	std::stringstream ss;
	ss << "BEGIN TRANSACTION;";
	std::string m = move_to_book_string( entry.m );
	ss << "INSERT OR REPLACE INTO book (position, move, forecast, searchdepth, eval_version) VALUES ((SELECT id FROM position WHERE pos='" << h << "'), '"
	   << m << "', "
	   << entry.forecast << ", "
	   << entry.search_depth << ", "
	   << eval_version
	   << ");";
	ss << "COMMIT TRANSACTION;";

	std::string query = ss.str();

	int res = sqlite3_exec( impl_->db, query.c_str(), 0, 0, 0 );

	if( res != SQLITE_OK ) {
		std::cerr << "Database failure" << std::endl;
		std::cerr << "Failed query: " << query << std::endl;
		abort();
	}
	return res == SQLITE_OK;
}

namespace {
extern "C" int all_positions_cb( void* p, int, char** data, char** /*names*/ )
{
	std::list<std::string>* wl = reinterpret_cast<std::list<std::string>*>(p);

	std::string pos = *data;
	if( pos.length() % 2 ) {
		return 1;
	}

	wl->push_back( pos );

	return 0;
}
}

void book::fold()
{
	std::list<std::string> positions_to_fold;

	{
		scoped_lock l(impl_->mtx);

		std::string query = "SELECT pos FROM position";

		int res = sqlite3_exec( impl_->db, query.c_str(), &all_positions_cb, reinterpret_cast<void*>(&positions_to_fold), 0 );
		if( res != SQLITE_OK && res != SQLITE_DONE ) {
			std::cerr << "Database failure" << std::endl;
			std::cerr << "Failed query: " << query << std::endl;
			abort();
		}
	}
}
