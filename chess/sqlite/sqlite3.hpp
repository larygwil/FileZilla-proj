#ifndef __SQLITE3_HPP__
#define __SQLITE3_HPP__

#include "sqlite3.h"

#include "../util/platform.hpp"
#include <string>
#include <vector>

class database
{
public:
	database( std::string const& file );

	virtual ~database();

	bool is_open() const;
	bool is_writable() const;
	bool open( std::string const& file );
	void close();

	int user_version();

	sqlite3* handle() { return db_; }


	void print_error( int code, std::string const& failed_query, char const* err_msg = 0 );
private:
	sqlite3* db_;
};

class statement
{
public:
	statement( database& db, std::string const& statement, bool report_errors = true );
	virtual ~statement();

	bool ok() const { return statement_ != 0; }

	// Binding values
	bool bind( int arg, std::string const& );
	bool bind( int arg, std::vector<unsigned char> const& );
	bool bind( int arg, uint64_t );

	// Executing query
	bool exec( bool report_errors = true, bool reset = true );
	bool exec( int (*callback)(void*,statement&), void* data, bool report_errors = true, bool reset = true );
	void reset();

	// Getting results
	int column_count();
	bool is_null( int col );
	std::vector<unsigned char> get_blob( int col );
	uint64_t get_int( int col );

	database& db() { return db_; }
private:
	database& db_;
	std::string query_;
	sqlite3_stmt* statement_;
};

class transaction
{
public:
	transaction( database& db );
	virtual ~transaction();

	bool init( bool report_errors = true );
	bool commit( bool report_errors = true );
	void rollback( bool report_errors = true );

private:
	database& db_;
	bool initialized_;
	bool released_;
};

#endif
