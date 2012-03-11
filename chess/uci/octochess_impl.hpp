
#ifndef OCTOCHESS_UCI_OCTOCHESS_IMPL_HEADER
#define OCTOCHESS_UCI_OCTOCHESS_IMPL_HEADER

#include "interface.hpp"

#include "../config.hpp"

namespace octochess {
namespace uci {

class octochess_uci : engine_interface {
public:
	octochess_uci( gui_interface_ptr const& );
	//callbacks
	virtual void new_game();
	virtual void set_position( std::string const& fen );
	virtual void make_moves( std::string const& list_of_moves );
	virtual void calculate( calculate_mode_type, position_time const& );
	virtual void stop();
	virtual void quit();

	//generic info
	virtual std::string name() const { return "Octochess"; }
	virtual std::string author() const { return "Tim Kosse"; }

private:
	class impl;
	std::shared_ptr<impl> impl_;
};

}
}

#endif
