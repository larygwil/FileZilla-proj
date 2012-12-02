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
		query("PRAGMA foreign_keys = ON", 0, 0 );
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

bool database::query( std::string const& query, int (*callback)(void*,int,char**,char**), void* data, bool report_errors )
{
	bool ret = false;

	char* err_msg = 0;
	int res = sqlite3_exec( handle(), query.c_str(), callback, data, &err_msg );

	if( res == SQLITE_OK || res == SQLITE_DONE ) {
		ret = true;
	}
	else if( report_errors ) {
		print_error( res, query, err_msg );
	}

	sqlite3_free( err_msg );

	return ret;
}


bool database::query_bind( std::string const& query, unsigned char const* data, uint64_t len, bool report_errors )
{
	sqlite3_stmt *statement = 0;
	int res = sqlite3_prepare_v2( handle(), query.c_str(), -1, &statement, 0 );
	if( res != SQLITE_OK ) {
		if( report_errors ) {
			print_error( res, query );
		}
		return false;
	}

	res = sqlite3_bind_blob( statement, 1, data, static_cast<int>(len), SQLITE_TRANSIENT );
	if( res != SQLITE_OK ) {
		if( report_errors ) {
			print_error( res, query );
		}
		return false;
	}

	do {
		res = sqlite3_step( statement );
	} while( res == SQLITE_BUSY || res == SQLITE_ROW );

	if( res != SQLITE_DONE ) {
		if( report_errors ) {
			print_error( res, query );
		}
		return false;
	}

	sqlite3_finalize( statement );

	return true;
}


bool database::query_row( std::string const& query, int (*callback)(void*,sqlite3_stmt*), void* data, bool report_errors )
{
	sqlite3_stmt *statement = 0;
	int res = sqlite3_prepare_v2( handle(), query.c_str(), -1, &statement, 0 );
	if( res != SQLITE_OK ) {
		if( report_errors ) {
			print_error( res, query );
		}
		sqlite3_finalize( statement );
		return false;
	}

	do {
		res = sqlite3_step( statement );
		if( res == SQLITE_ROW ) {
			if( callback( data, statement ) ) {
				if( report_errors ) {
					std::cerr << "Callback requested query abort." << std::endl;
					std::cerr << "Failed query: " << query << std::endl;
					abort();
				}
				sqlite3_finalize( statement );
				return false;
			}
		}
	} while( res == SQLITE_BUSY || res == SQLITE_ROW );

	bool ret = false;
	if( res != SQLITE_DONE && res != SQLITE_OK ) {
		if( report_errors ) {
			print_error( res, query );
		}
	}
	else {
		ret = true;
	}

	sqlite3_finalize( statement );

	return ret;
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
}


bool statement::exec( bool report_errors ) {
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
	} while( res == SQLITE_BUSY || res == SQLITE_ROW );

	if( res != SQLITE_OK && res != SQLITE_DONE ) {
		if( report_errors ) {
			db_.print_error( res, query_, 0 );
		}
	}
	else {
		ret = true;
	}

	sqlite3_reset( statement_ );

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
