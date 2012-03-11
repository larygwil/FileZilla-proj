#ifndef OCTOCHESS_UCI_INTERFACE_HEADER
#define OCTOCHESS_UCI_INTERFACE_HEADER

#include "types.hpp"

#include <map>
#include <memory>
#include <string>

namespace octochess {
namespace uci {

namespace calculate_mode {
	enum type {
		forced,
		infinite
	};
}
typedef calculate_mode::type calculate_mode_type;
class position_time;

//GUI -> ENGINE
class engine_interface {
public:
	//callbacks
	virtual void new_game() = 0;
	virtual void set_position( std::string const& fen ) = 0;
	virtual void make_moves( std::string const& list_of_moves ) = 0;
	virtual void calculate( calculate_mode_type, position_time const& ) = 0;
	virtual void stop() = 0;
	virtual void quit() = 0;

	//generic info
	virtual std::string name() const = 0;
	virtual std::string author() const = 0;

	virtual ~engine_interface() {}
};
typedef std::shared_ptr<engine_interface> engine_interface_ptr;

class info;

//ENGINE -> GUI
class gui_interface {
public:
	virtual void set_engine_interface( engine_interface& ) = 0;

	virtual void tell_best_move( std::string const& move ) = 0;
	virtual void tell_info( info const& ) = 0;
	
	virtual ~gui_interface() {}
};
typedef std::shared_ptr<gui_interface> gui_interface_ptr;

}
}

#endif
