#ifndef OCTOCHESS_UCI_INTERFACE_HEADER
#define OCTOCHESS_UCI_INTERFACE_HEADER

#include "types.hpp"

#include <map>
#include <memory>
#include <string>

#if _MSC_VER && _MSC_VER < 1600
namespace std {
namespace tr1 {
	// Stupid hack to get tr1::shared_ptr into std
}
using namespace tr1;
}
#endif

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
	virtual void calculate( calculate_mode_type, position_time const&, uint64_t depth ) = 0;
	virtual void stop() = 0;
	virtual void quit() = 0;

	//generic info
	virtual std::string name() const = 0;
	virtual std::string author() const = 0;

	// options
	virtual uint64_t get_hash_size() const = 0; // In MiB
	virtual uint64_t get_min_hash_size() const = 0; // In MiB
	virtual void set_hash_size( uint64_t mb ) = 0;
	virtual unsigned int get_threads() const = 0;
	virtual unsigned int get_max_threads() const = 0;
	virtual void set_threads( unsigned int threads ) = 0;
	virtual bool use_book() const = 0;
	virtual void use_book( bool use ) = 0;

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
