#include "book.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include "sqlite/sqlite3.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

int const eval_version = 5;

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

std::string history_to_string( std::vector<move>::const_iterator const& begin, std::vector<move>::const_iterator const& end )
{
	std::string ret;
	for( std::vector<move>::const_iterator it = begin; it != end; ++it ) {
		ret += move_to_book_string(*it);
	}

	return ret;
}
}


book_entry::book_entry()
	: forecast()
	, search_depth()
	, eval_version()
	, result_in_book()
	, folded_forecast()
	, folded_searchdepth()
{
}


class book::impl
{
public:
	impl() : db()
	{
	}

	bool query( std::string const& query, int (*callback)(void*,int,char**,char**), void* data, bool report_errors = true );

	mutex mtx;
	sqlite3* db;

	std::ofstream logfile;
};


bool book::impl::query( std::string const& query, int (*callback)(void*,int,char**,char**), void* data, bool report_errors )
{
	bool ret = false;

	char* err_msg = 0;
	int res = sqlite3_exec( db, query.c_str(), callback, data, &err_msg );

	if( res == SQLITE_OK || res == SQLITE_DONE ) {
		ret = true;
	}
	else if( report_errors ) {
		std::cerr << "Database failure" << std::endl;
		std::cerr << "Error code: " << res << std::endl;
		if( err_msg ) {
			std::cerr << "Error string: " << err_msg << std::endl;
		}
		std::cerr << "Failed query: " << query << std::endl;
		abort();
	}
	
	sqlite3_free( err_msg );

	return ret;
}


book::book( std::string const& book_dir )
	: impl_( new impl )
{
	std::string fn( book_dir + "opening_book.db" );
	if( sqlite3_open_v2( fn.c_str(), &impl_->db, SQLITE_OPEN_READWRITE, 0 ) != SQLITE_OK ) {
		sqlite3_close( impl_->db );
		impl_->db = 0;
	}
	if( impl_->db ) {
		sqlite3_busy_timeout( impl_->db, 5000 );
		impl_->query("PRAGMA foreign_keys = ON", 0, 0 );
	}
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
	cb_data()
		: c()
		, pm()
		, found_()
	{
	}

	std::vector<book_entry>* entries;
	position p;
	color::type c;

	move_info moves[200];
	move_info* pm;

	int found_;
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


bool conv_to_move_slow( position const& p, color::type c, move& m, char const* data, bool print_errors ) {
	unsigned char si = conv_to_index( data[0] );
	unsigned char ti = conv_to_index( data[1] );

	char ms[5] = {0};
	ms[0] = (si % 8) + 'a';
	ms[1] = (si / 8) + '1';
	ms[2] = (ti % 8) + 'a';
	ms[3] = (ti / 8) + '1';

	if( !parse_move( p, c, ms, m, print_errors ) ) {
		return false;
	}

	return true;
}

bool conv_to_move( char const* data, move& m, move_info const* begin, move_info const* end ) {
	unsigned char si = conv_to_index( data[0] );
	unsigned char ti = conv_to_index( data[1] );

	for( ; begin != end; ++begin ) {
		if( begin->m.source == si && begin->m.target == ti ) {
			m = begin->m;
			return true;
		}
	}

	return false;
}

extern "C" int get_cb( void* p, int, char** data, char** /*names*/ ) {
	cb_data* d = reinterpret_cast<cb_data*>(p);

	if( !d->pm ) {
		check_map check;
		calc_check_map( d->p, d->c, check );
		d->pm = d->moves;
		calculate_moves( d->p, d->c, d->pm, check );
	}

	if( ++d->found_ > (d->pm - d->moves) ) {
		return 1;
	}

	move m;
	if( !conv_to_move( data[0], m, d->moves, d->pm ) ) {
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
	int evalversion = atoi(data[3]);
	if( evalversion < 0 ) {
		return 1;
	}
	int folded_forecast = forecast;
	int folded_searchdepth = searchdepth;
	if( data[4] && data[5] ) {
		folded_forecast = atoi(data[4]);
		if( folded_forecast > 32767 || folded_forecast < -32768 ) {
			return 1;
		}
		folded_searchdepth = atoi(data[5]);
		if( folded_searchdepth < 1 || folded_searchdepth > 40 ) {
			return 1;
		}
	}
	else if( data[4] || data[5] ) {
		return 1;
	}

	book_entry entry;
	entry.forecast = static_cast<short>(forecast);
	entry.search_depth = static_cast<short>(searchdepth);
	entry.eval_version = static_cast<short>(evalversion);
	entry.folded_forecast = static_cast<short>(folded_forecast);
	entry.folded_searchdepth = static_cast<short>(folded_searchdepth);
	entry.m = m;
	d->entries->push_back( entry );

	return 0;
}
}


std::vector<book_entry> book::get_entries( position const& p, color::type c, std::vector<move> const& history, int move_limit, bool allow_transpositions )
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

	std::string hs = history_to_string( history );

	std::stringstream ss;
	ss << "SELECT move, forecast, searchdepth, eval_version, folded_forecast, folded_depth FROM book WHERE position = (SELECT id FROM position WHERE pos ='" << hs << "') ORDER BY forecast DESC,searchdepth DESC";

	if( move_limit != -1 ) {
		ss << " LIMIT " << move_limit;
	}

	impl_->query( ss.str(), &get_cb, reinterpret_cast<void*>(&data) );

	if( ret.empty() && allow_transpositions ) {
		uint64_t hash = get_zobrist_hash( p );

		std::stringstream ss;
		ss << "SELECT move, forecast, searchdepth, eval_version, folded_forecast, folded_depth FROM book WHERE position = (SELECT id FROM position WHERE hash=" << static_cast<sqlite3_int64>(hash) << " LIMIT 1) ORDER BY forecast DESC,searchdepth DESC";

		if( move_limit != -1 ) {
			ss << " LIMIT " << move_limit;
		}

		if( impl_->query( ss.str(), &get_cb, reinterpret_cast<void*>(&data), false ) ) {
			if( !ret.empty() ) {
				std::cerr << "Found transposition" << std::endl;
			}
		}
		else {
			ret.clear();
		}
	}

	if( !ret.empty() && data.found_ != (data.pm - data.moves) ) {
		if( move_limit == -1 || allow_transpositions ) {
			ret.clear();
		}
	}

	std::sort( ret.begin(), ret.end(), SortFolded() );

	return ret;
}


bool book::add_entries( std::vector<move> const& history, std::vector<book_entry> entries )
{
	std::sort( entries.begin(), entries.end() );

	std::string hs = history_to_string( history );

	position p;
	init_board( p );
	color::type c = color::white;

	for( std::vector<move>::const_iterator it = history.begin(); it != history.end(); ++it ) {
		apply_move( p, *it, c );
		c = static_cast<color::type>(1-c);
	}
	uint64_t hash = get_zobrist_hash( p );

	std::stringstream ss;
	ss << "BEGIN TRANSACTION;";
	ss << "INSERT OR IGNORE INTO position (pos, hash) VALUES ('" << hs << "', " << static_cast<sqlite3_int64>(hash) << ");";
	for( std::vector<book_entry>::const_iterator it = entries.begin(); it != entries.end(); ++it ) {
		std::string m = move_to_book_string( it->m );
		ss << "INSERT OR REPLACE INTO book (position, move, forecast, searchdepth, eval_version) VALUES ((SELECT id FROM position WHERE pos='" << hs << "'), '"
		   << m << "', "
		   << it->forecast << ", "
		   << it->search_depth << ", "
		   << eval_version
		   << ");";
	}
	ss << "COMMIT TRANSACTION;";

	scoped_lock l(impl_->mtx);

	impl_->logfile << ss.str();
	impl_->logfile.flush();

	return impl_->query( ss.str(), 0, 0 );
}


namespace {
extern "C" int count_cb( void* p, int, char** data, char** /*names*/ ) {
	uint64_t* count = reinterpret_cast<uint64_t*>(p);
	*count = atoll( *data );

	return 0;
}
}


uint64_t book::size()
{
	scoped_lock l(impl_->mtx);

	if( !impl_->db ) {
		return 0;
	}

	std::string query = "SELECT COUNT(id) FROM position;";

	uint64_t count = 0;
	impl_->query( query, &count_cb, reinterpret_cast<void*>(&count) );

	return count;
}


void book::mark_for_processing( std::vector<move> const& history )
{
	scoped_lock l(impl_->mtx);

	if( !impl_->db ) {
		return;
	}

	std::stringstream ss;

	ss << "BEGIN TRANSACTION;";

	position p;
	init_board( p );
	color::type c = color::white;

	for( std::vector<move>::const_iterator it = history.begin(); it != history.end(); ++it ) {
		apply_move( p, *it, c );
		c = static_cast<color::type>(1-c);
		uint64_t hash = get_zobrist_hash( p );

		std::string hs = history_to_string( history.begin(), it + 1 );
		ss << "INSERT OR IGNORE INTO position (pos, hash) VALUES ('" << hs << "', " << static_cast<sqlite3_int64>(hash) << ");";
	}

	ss << "COMMIT TRANSACTION;";

	impl_->query( ss.str(), 0, 0 );
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
		if( !conv_to_move_slow( w.p, w.c, m, ms.c_str(), true ) ) {
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

	impl_->query( query, &work_cb, reinterpret_cast<void*>(&ret) );
		
	return ret;
}


std::list<book_entry_with_position> book::get_all_entries( int move_limit )
{
	std::list<book_entry_with_position> ret;

	std::list<work> positions;

	{
		scoped_lock l(impl_->mtx);

		std::string query = "SELECT pos FROM position ORDER BY LENGTH(pos)";

		impl_->query( query, &work_cb, reinterpret_cast<void*>(&positions) );
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

	return impl_->query( ss.str(), 0, 0 );
}

namespace {
extern "C" int fold_forecast( void* p, int, char** data, char** /*names*/ )
{
	std::pair<int, int> *ret = reinterpret_cast<std::pair<int, int>*>(p);
	ret->first = atoi(data[0]);
	ret->second = atoi(data[1]);

	return 0;
}

extern "C" int fold_position( void* p, int, char** data, char** /*names*/ )
{
	book::impl* impl_ = reinterpret_cast<book::impl*>(p);
	std::string id = data[0];
	std::string pos = data[1];
	if( pos.length() % 2 ) {
		return 1;
	}

	if( !pos.length() ) {
		return 0;
	}

	std::pair<int, int> best;
	std::string query = "SELECT folded_forecast, folded_depth FROM book WHERE position = " + id + " ORDER BY folded_forecast DESC, folded_depth ASC LIMIT 1;";
	if( !impl_->query( query, &fold_forecast, &best ) ) {
		return 1;
	}

	if( !best.second ) {
		// This position is not yet in book.
		return 0;
	}

	std::string parent = pos.substr( 0, pos.length() - 2 );
	std::string move = pos.substr( pos.length() - 2 );

	std::stringstream ss;
	ss << "UPDATE book SET folded_forecast = " << -best.first << ", folded_depth = " << best.second + 1 << " WHERE position = (SELECT id FROM position WHERE pos = '" << parent << "') AND move = '" << move << "';";

	if( !impl_->query( ss.str(), 0, 0 ) ) {
		return 1;
	}

	return 0;
}
}

void book::fold()
{
	scoped_lock l(impl_->mtx);

	uint64_t max_length = 0;
	std::string query = "BEGIN TRANSACTION; SELECT LENGTH(pos) FROM position ORDER BY LENGTH(pos) DESC LIMIT 1;";
	if( !impl_->query( query, &count_cb, &max_length ) ) {
		return;
	}

	std::cerr << "Resetting folded forcasts to actual forecasts...";
	query = "UPDATE book set folded_depth = searchdepth;";
	if( !impl_->query( query, 0, 0 ) ) {
		return;
	}
	std::cerr << " done" << std::endl;

	std::cerr << "Resetting folded depths to actual search depths...";
	query = "UPDATE book set folded_forecast = forecast;";
	if( !impl_->query( query, 0, 0 ) ) {
		return;
	}
	std::cerr << " done" << std::endl;

	std::cerr << "Folding";
	for( uint64_t i = max_length; i > 0; i -= 2 ) {
		std::cerr << ".";
		std::stringstream ss;
		ss << "SELECT id, pos FROM position WHERE length(pos) = " << i << ";";
		impl_->query( ss.str(), &fold_position, impl_ );
	}
	std::cerr << " done" << std::endl;

	std::cerr << "Comitting...";
	query = "COMMIT TRANSACTION;";
	if( !impl_->query( query, 0, 0 ) ) {
		return;
	}
	std::cerr << " done" << std::endl;
}


namespace {
extern "C" int stats_processed_cb( void* p, int, char** data, char** /*names*/ ) {
	book_stats* stats = reinterpret_cast<book_stats*>(p);

	int64_t depth = atoll( data[0] ) / 2;
	int64_t processed = atoll( data[1] );

	if( depth > 0 && processed > 0 ) {
		stats->data[depth].processed = static_cast<uint64_t>(processed);
		stats->total_processed += static_cast<uint64_t>(processed);
	}

	return 0;
}

extern "C" int stats_queued_cb( void* p, int, char** data, char** /*names*/ ) {
	book_stats* stats = reinterpret_cast<book_stats*>(p);

	int64_t depth = atoll( data[0] ) / 2;
	int64_t queued = atoll( data[1] );

	if( depth > 0 && queued > 0 ) {
		stats->data[depth].queued = static_cast<uint64_t>(queued);
		stats->total_queued += static_cast<uint64_t>(queued);
	}

	return 0;
}
}

book_stats book::stats()
{
	book_stats ret;

	scoped_lock l(impl_->mtx);

	std::string query = "SELECT length(pos), count(pos) FROM position WHERE id IN (SELECT DISTINCT(position) FROM book) GROUP BY LENGTH(pos) ORDER BY LENGTH(pos)";
	impl_->query( query, &stats_processed_cb, reinterpret_cast<void*>(&ret) );
	
	query = "SELECT length(pos), count(pos) FROM position WHERE id NOT IN (SELECT DISTINCT(position) FROM book) GROUP BY LENGTH(pos) ORDER BY LENGTH(pos)";
	impl_->query( query, &stats_queued_cb, reinterpret_cast<void*>(&ret) );

	return ret;
}


bool book::set_insert_logfile( std::string const& log_file )
{
	scoped_lock l(impl_->mtx);

	impl_->logfile.close();
	if( !log_file.empty() ) {
		impl_->logfile.open( log_file.c_str(), std::ofstream::out|std::ofstream::app );
	}

	return impl_->logfile.is_open();
}


std::string entries_to_string( std::vector<book_entry> const& entries )
{
	std::ostringstream out;
	out << "  Move       Folded      Current" << std::endl;
	for( std::vector<book_entry>::const_iterator it = entries.begin(); it != entries.end(); ++it ) {
		out << move_to_string( it->m )
			<< std::setw(7) << it->folded_forecast << " @ " << std::setw(2) << it->folded_searchdepth
			<< std::setw(7) << it->forecast << " @ " << std::setw(2) << it->search_depth
			<< std::endl;
	}

	return out.str();
}
