#ifndef OCTOCHESS_UCI_INFO_HEADER
#define OCTOCHESS_UCI_INFO_HEADER

#include "types.hpp"

#include <vector>
#include <string>

namespace octochess {
namespace uci {

class info {
	typedef std::vector< std::pair<std::string, std::string> > container_type;
public:
	typedef container_type::const_iterator const_iterator;

	// search depth in plies
	void depth( uint );
		
	// the time searched in ms, this should be sent together with the principal variation.
	void time_spent( time );

	// the best line found
	void principal_variation( std::string const& line );

	// x nodes searched, the engine should send this info regularly
	void node_count( uint );
	
	//the score from the engine's point of view in centipawns.
	void score( int evaluation );
	void mate_in_n_moves( int moves );
	
	void nodes_per_second( uint );

	const_iterator begin() const { return values_.begin(); };
	const_iterator end() const { return values_.end(); };
private:
	template<typename T>
	void set_value( std::string const& name, T const& v );
private:
	container_type values_;
};

}
}

#endif
