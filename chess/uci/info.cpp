#include "info.hpp"

#include <sstream>

namespace octochess {
namespace uci {

void info::depth( uint v ) {
	set_value( "depth", v );
}
		
void info::time_spent( time v ) {
	set_value( "time", v );
}

void info::principal_variation( std::string const& v ) {
	set_value( "pv", v );
}

void info::node_count( uint v ) {
	set_value( "nodes", v );
}

void info::score( int v ) {
	set_value( "score cp", v );
}

void info::mate_in_n_moves( int v ) {
	set_value( "score mate", v );
}
	
void info::nodes_per_second( uint v ) {
	set_value( "nps", v );
}

template<typename T>
void info::set_value( std::string const& name, T const& v ) {
	std::ostringstream out;
	out << v;
	values_.push_back( std::make_pair(name, out.str() ) );
}

}
}
