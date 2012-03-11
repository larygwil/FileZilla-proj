
#include "minimalistic_uci_protocol.hpp"
#include "info.hpp"
#include "time.hpp"

#include "../logger.hpp"

#include <cassert>
#include <iostream>
#include <sstream>

namespace octochess {
namespace uci {

minimalistic_uci_protocol::minimalistic_uci_protocol()
	: connected_(false)
{}

void minimalistic_uci_protocol::thread_entry_point() {
	std::string line;
	while( std::getline(std::cin, line) ) {
		logger::log_input(line);
		if( connected_ ) {
			parse_command( line );
		} else {
			parse_init( line );
		}
	}
}

void minimalistic_uci_protocol::parse_init( std::string const& line ) {
	if( line == "uci" ) {
		assert( callbacks_ && "callbacks not set" );
		identify( callbacks_->name(), callbacks_->author() );
		//send options in future version
		std::cout << "uciok" << std::endl;
		connected_ = true;
	} else {
		std::cerr << "unknown command when not connected: " << line << std::endl;
	}
}

void minimalistic_uci_protocol::parse_command( std::string const& line ) {
	if( line == "isready" ) {
		std::cout << "readyok" << std::endl;
	} else if( line == "quit" ) {
		callbacks_->quit();
		connected_ = false;
	} else if( line == "stop" ) {
		callbacks_->stop();
	} else if( line == "ucinewgame" ) {
		callbacks_->new_game();
	} else if( line.substr(0, 9) == "position " ) {
		handle_position( line.substr(9) );
	} else if( line.substr(0, 3) == "go " ) {
		handle_go( line.substr(3) );
	} else {
		std::cerr << "unknown command when connected: " << line << std::endl;
	}
}

void minimalistic_uci_protocol::handle_position( std::string const& params ) {
	std::string::size_type pos = params.find("moves"); //there should be always this string

	if( params.substr(0,3) == "fen" ) {
		callbacks_->set_position( params.substr(4, pos) );
	} else if( params.substr(0,8) == "startpos" ) {
		callbacks_->set_position( std::string() );
	} else {
		std::cerr << "unknown parameter with position command: " << params << std::endl;
	}

	if( pos != std::string::npos ) {
		callbacks_->make_moves( params.substr( pos+6 ) );
	}
}

namespace {
	template<typename T>
	T extract( std::istream& in ) {
		T v = T();
		in >> v;
		return v;
	}
}

void minimalistic_uci_protocol::handle_go( std::string const& params ) {
	calculate_mode_type mode = calculate_mode::forced;

	if( params.find("infinite") != std::string::npos ) {
		mode = calculate_mode::infinite;
	}
	position_time t;

	std::istringstream in( params );
	std::string cmd;
	while( in >> cmd ) {
		if( cmd == "wtime" ) {
			t.set_white_time( extract<uint>(in) );
		} else if( cmd == "btime" ) {
			t.set_black_time( extract<uint>(in) );
		} else if( cmd == "winc" ) {
			t.set_white_increment( extract<uint>(in) );
		} else if( cmd == "winc" ) {
			t.set_black_increment( extract<uint>(in) );
		}
	}

	callbacks_->calculate( mode, t );
}

void minimalistic_uci_protocol::set_engine_interface( engine_interface& p ) {
	callbacks_ = &p;
}

void minimalistic_uci_protocol::identify( std::string const& name, std::string const& author ) {
	std::cout << "id name " << name << '\n' << "id author " << author << std::endl;
}

void minimalistic_uci_protocol::tell_best_move( std::string const& move ) {
	std::cout << "bestmove " << move << std::endl;
}

void minimalistic_uci_protocol::tell_info( info const& i ) {
	std::cout << "info ";
	for( info::const_iterator it = i.begin(); it != i.end(); ++it ) {
		std::cout << it->first << ' ' << it->second << ' ';
	}
	std::cout << std::endl;
}

}
}

