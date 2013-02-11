#include "logger.hpp"

#include <iostream>
#include <fstream>

namespace logger {

namespace {
std::ofstream logfile;
bool show_debug_ = true;
}

class logbuf : public std::streambuf
{
public:
	logbuf( std::streambuf* orig )
		: orig_(orig)
	{
		orig_->pubsetbuf( buffer_, 1024 );
	}

	virtual ~logbuf()
	{
		orig_->pubsetbuf( 0, 0 );
	}

	virtual std::streamsize xsputn( std::streambuf::char_type const* s, std::streamsize n )
	{
		if( logfile.is_open() ) {
			logfile.write( s, n );
		}
		return orig_->sputn( s, n );
	}

	virtual int overflow( int c )
	{
		if (c == EOF) {
			return !EOF;
		}
		else {
			if( logfile.is_open() ) {
				logfile.put( c );
			}
			return orig_->sputc( c ) == EOF;
		}
	}

	virtual int sync()
	{
		int r = orig_->pubsync();
		return r == 0 ? 0 : -1;
	}

	std::streambuf* orig_;

	char buffer_[1024];
};




namespace {
logbuf* new_cerr = 0;
logbuf* new_cout = 0;
std::streambuf* original_cerr = 0;
std::streambuf* original_cout = 0;
}



void init( std::string const& fn )
{
	if( original_cerr ) {
		return;
	}

	if( !fn.empty() ) {
		logfile.open( fn.c_str(), std::ios_base::app );
		logfile.setf( std::ios::unitbuf );
	}

	original_cerr = std::cerr.rdbuf();
	original_cout = std::cout.rdbuf();

	new_cerr = new logbuf( original_cerr );
	new_cout = new logbuf( original_cout );

	std::cerr.rdbuf( new_cerr );
	std::cout.rdbuf( new_cout );
}


void log_input( std::string const& in )
{
	if( logfile.is_open() ) {
		logfile << in.c_str() << std::endl;
	}
}


void cleanup()
{
	if( original_cerr ) {
		std::cerr.rdbuf( original_cerr );
		original_cerr = 0;
	}
	if( original_cout ) {
		std::cout.rdbuf( original_cout );
		original_cout = 0;
	}

	delete new_cerr;
	delete new_cout;
	new_cerr = 0;
	new_cout = 0;

	logfile.close();
}


void show_debug( bool show )
{
	show_debug_ = show;
}

bool show_debug()
{
	return show_debug_;
}
}

std::ostream& dlog()
{
	if( logger::show_debug_ ) {
		return std::cerr;
	}
	else {
		return logger::logfile;
	}
}
