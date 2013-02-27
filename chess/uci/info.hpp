#ifndef OCTOCHESS_UCI_INFO_HEADER
#define OCTOCHESS_UCI_INFO_HEADER

#include "types.hpp"

#include <vector>
#include <string>

class duration;

namespace octochess {
namespace uci {

class info {
	typedef std::vector< std::pair<std::string, std::string> > container_type;
public:
	typedef container_type::const_iterator const_iterator;

	// search depth in plies
	void depth( uint );

	// selective (e.g. after check extensions) search depth in plies
	void selective_depth( uint );

	// the duration searched, this should be sent together with the principal variation.
	void time_spent( duration const& );

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

	void multipv( unsigned int mpv );
private:
	template<typename T>
	void set_value( std::string const& name, T const& v );
private:
	container_type values_;
};

}
}

#endif
