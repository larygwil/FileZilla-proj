#include "logger.hpp"

#include <iostream>
#include <fstream>

namespace logger {

class logbuf : public std::streambuf
{
public:
	logbuf( std::streambuf* orig, std::string const& fn )
		: orig_(orig)
	{
		if( !fn.empty() ) {
			logfile_.open( fn.c_str(), std::ios_base::app );
			logfile_.setf( std::ios::unitbuf );
		}

		orig_->pubsetbuf( buffer_, 1024 );
	}

	virtual ~logbuf()
	{
		orig_->pubsetbuf( 0, 0 );
	}

	virtual std::streamsize xsputn( std::streambuf::char_type const* s, std::streamsize n )
	{
		logfile_.write( s, n );
		return orig_->sputn( s, n );
	}

	virtual int overflow( int c )
	{
		if (c == EOF) {
			return !EOF;
		}
		else {
			if( logfile_.is_open() ) {
				logfile_.put( c );
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
	std::ofstream logfile_;

	char buffer_[1024];
};

namespace {
logbuf* new_cerr = 0;
logbuf* new_cout = 0;
std::streambuf* original_cerr = 0;
std::streambuf* original_cout = 0;
}



void init( std::string const& fn ) {
	original_cerr = std::cerr.rdbuf();
	original_cout = std::cout.rdbuf();

	new_cerr = new logbuf( original_cerr, fn );
	new_cout = new logbuf( original_cout, fn );

	std::cerr.rdbuf( new_cerr );
	std::cout.rdbuf( new_cout );	
}


void log_input( std::string const& in )
{
	if( new_cout && new_cout->logfile_.is_open() ) {
		new_cout->logfile_ << in.c_str() << std::endl;
	}
}


void cleanup()
{
	if( original_cerr ) {
		std::cerr.rdbuf( original_cerr );
	}
	if( original_cout ) {
		std::cout.rdbuf( original_cout );
	}

	delete new_cerr;
	delete new_cout;
}

}
