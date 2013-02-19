#include "sqlite3.hpp"

#include <iostream>
#include <string>

database::database( std::string const& fn )
	: db_()
{
	open( fn );
}


database::~database()
{
	sqlite3_close( db_ );
}


bool database::is_open() const
{
	return db_ != 0;
}


bool database::is_writable() const
{
	return is_open() && sqlite3_db_readonly(db_, "main") == 0;
}


bool database::open( std::string const& fn )
{
	close();

	if( sqlite3_open_v2( fn.c_str(), &db_, SQLITE_OPEN_READWRITE, 0 ) != SQLITE_OK ) {
		sqlite3_close( db_ );
		db_ = 0;
	}
	if( db_ ) {
		sqlite3_busy_timeout( db_, 5000 );
		statement s( *this, "PRAGMA foreign_keys = ON");
		s.exec();
	}

	return is_open();
}


void database::close()
{
	sqlite3_close( db_ );
	db_ = 0;
}


void database::print_error( int code, std::string const& failed_query, char const* err_msg )
{
	std::cerr << "Database failure" << std::endl;
	std::cerr << "Error code: " << code << std::endl;
	if( err_msg ) {
		std::cerr << "Error string: " << err_msg << std::endl;
	}
	else {
		sqlite3_mutex_enter( sqlite3_db_mutex( db_ ) );
		err_msg = sqlite3_errmsg( db_ );
		if( err_msg ) {
			std::cerr << "Error string: " << err_msg << std::endl;
		}
		sqlite3_mutex_leave( sqlite3_db_mutex( db_ ) );
	}
	if( !failed_query.empty() ) {
		std::cerr << "Failed query: " << failed_query << std::endl;
	}
	abort();
}

namespace {
int version_cb( void* p, statement& s ) {
	int* v = reinterpret_cast<int*>(p);
	*v = s.get_int(0);

	return 0;
}
}

int database::user_version()
{
	if( !is_open() ) {
		return -1;
	}

	int v = -1;
	statement s( *this, "PRAGMA user_version" );
	if( !s.exec( version_cb, &v ) ) {
		v = -1;
	}

	return v;
}


statement::statement( database& db, std::string const& statement, bool report_errors )
	: db_(db)
	, query_( statement )
	, statement_()
{
	if( db_.is_open() ) {
		int res = sqlite3_prepare_v2( db_.handle(), statement.c_str(), -1, &statement_, 0 );
		if( res != SQLITE_OK && res != SQLITE_DONE ) {
			if( report_errors ) {
				db_.print_error( res, statement, 0 );
			}
			sqlite3_finalize( statement_ );
			statement_ = 0;
		}
	}
	else {
		if( report_errors ) {
			std::cerr << "Trying to prepare statement on a database that is not open." << std::endl;
			std::cerr << "Failed query: " << statement << std::endl;
		}
	}
}

statement::~statement()
{
	sqlite3_finalize( statement_ );
}


bool statement::exec( bool report_errors, bool reset ) {
	return exec( 0, 0, report_errors, reset );
}


bool statement::exec( int (*callback)(void*,statement&), void* data, bool report_errors, bool reset ) {
	if( !statement_ ) {
		if( report_errors ) {
			std::cerr << "Calling exec on an unprepared statement." << std::endl;
			std::cerr << "Failed query: " << query_ << std::endl;
		}
		return false;
	}

	bool ret = false;

	int res;
	do {
		res = sqlite3_step( statement_ );

		if( res == SQLITE_ROW ) {
			if( callback && callback( data, *this ) ) {
				if( report_errors ) {
					std::cerr << "Callback requested query abort." << std::endl;
					abort();
				}
				return false;
			}
		}
	} while( res == SQLITE_BUSY || res == SQLITE_ROW );

	if( res != SQLITE_OK && res != SQLITE_DONE ) {
		if( report_errors ) {
			db_.print_error( res, query_, 0 );
		}
	}
	else {
		ret = true;
	}

	if( reset ) {
		sqlite3_reset( statement_ );
	}

	return ret;
}


void statement::reset()
{
	if( statement_ ) {
		sqlite3_reset( statement_ );
	}
}


bool statement::bind( int arg, std::string const& s)
{
	if( !statement_ ) {
		return false;
	}

	int res = sqlite3_bind_text( statement_, arg, s.c_str(), s.size(), SQLITE_TRANSIENT );
	if( res != SQLITE_OK ) {
		db_.print_error( res, std::string() );
	}

	return res == SQLITE_OK;
}


bool statement::bind( int arg, std::vector<unsigned char> const& v)
{
	if( !statement_ ) {
		return false;
	}

	unsigned char const* p;
	unsigned char dummy;
	if( v.empty() ) {
		p = &dummy;
	}
	else {
		p = &v[0];
	}
	int res = sqlite3_bind_blob( statement_, arg, p, v.size(), SQLITE_TRANSIENT );
	if( res != SQLITE_OK ) {
		db_.print_error( res, std::string() );
	}

	return res == SQLITE_OK;
}


bool statement::bind( int arg, uint64_t v)
{
	if( !statement_ ) {
		return false;
	}

	int res = sqlite3_bind_int64( statement_, arg, static_cast<int64_t>(v) );
	if( res != SQLITE_OK ) {
		db_.print_error( res, std::string() );
	}

	return res == SQLITE_OK;
}


int statement::column_count()
{
	if( !statement_ ) {
		return 0;
	}

	return sqlite3_column_count(statement_);
}


bool statement::is_null( int col )
{
	if( !statement_ ) {
		return true;
	}

	return sqlite3_column_type( statement_, col ) == SQLITE_NULL;
}


std::vector<unsigned char> statement::get_blob( int col )
{
	std::vector<unsigned char> ret;

	if( statement_ ) {
		const void* b = sqlite3_column_blob( statement_, col );
		if( b ) {
			int len = sqlite3_column_bytes( statement_, col );
			if( len > 0 ) {
				unsigned char const* p = reinterpret_cast<unsigned char const*>(b);
				ret = std::vector<unsigned char>( p, p + len );
			}
		}
	}

	return ret;
}


uint64_t statement::get_int( int col )
{
	uint64_t ret = 0;
	if( statement_ ) {
		ret = static_cast<uint64_t>( sqlite3_column_int64( statement_, col ) );
	}

	return ret;
}


transaction::transaction( database& db )
	: db_( db )
	, initialized_()
	, released_()
{
}


bool transaction::init( bool report_errors )
{
	if( initialized_ ) {
		if( report_errors ) {
			std::cerr << "Trying to initialize same transaction more than once." << std::endl;
		}
		return false;
	}
	statement s( db_, "SAVEPOINT \"transaction\";", report_errors );
	bool ret = s.exec();
	if( ret ) {
		initialized_ = true;
	}

	return ret;
}


bool transaction::commit( bool report_errors )
{
	if( !initialized_ ) {
		if( report_errors ) {
			std::cerr << "Trying to commit transaction that hasn't been initialized." << std::endl;
		}
		return false;
	}

	if( released_ ) {
		if( report_errors ) {
			std::cerr << "Trying to commit transaction that has already been released." << std::endl;
		}
	}

	statement release( db_, "RELEASE SAVEPOINT \"transaction\";", report_errors );
	bool ret = release.exec( report_errors );

	released_ = true;

	return ret;
}


void transaction::rollback( bool report_errors )
{
	if( !initialized_ ) {
		if( report_errors ) {
			std::cerr << "Trying to commit transaction that hasn't been initialized." << std::endl;
		}
		return;
	}

	if( released_ ) {
		if( report_errors ) {
			std::cerr << "Trying to commit transaction that has already been released." << std::endl;
		}
	}

	statement rollback( db_, "ROLLBACK TO SAVEPOINT \"transaction\";", report_errors );
	rollback.exec();

	statement release( db_, "RELEASE SAVEPOINT \"transaction\";", report_errors );
	release.exec();

	released_ = true;

	return;
}


transaction::~transaction()
{
	if( initialized_ && !released_ ) {
		rollback();
	}
}
