#ifndef __SQLITE3_HPP__
#define __SQLITE3_HPP__

#include "sqlite3.h"

#include "../util/platform.hpp"
#include <string>

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

	bool query( std::string const& query, int (*callback)(void*,int,char**,char**), void* data, bool report_errors = true );

	// Mainly for inserting a single blob
	bool query_bind( std::string const& query, unsigned char const* data, uint64_t len, bool report_errors = true );

	// Mainly for parsing rows containing a blob
	bool query_row( std::string const& query, int (*callback)(void*,sqlite3_stmt*), void* data, bool report_errors = true );

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

	bool exec( bool report_errors = true );
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
