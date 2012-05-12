#ifndef __SQLITE3_HPP__
#define __SQLITE3_HPP__

#include "sqlite3.h"

#include "../platform.hpp"
#include <string>

class database
{
public:
	database( std::string const& file );

	virtual ~database();

	bool is_open() const;

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


#endif
