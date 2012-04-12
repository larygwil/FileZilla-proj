
#ifndef OCTOCHESS_UCI_MINIMALISTIC_UCI_PROTOCOL_HEADER
#define OCTOCHESS_UCI_MINIMALISTIC_UCI_PROTOCOL_HEADER

#include "interface.hpp"

namespace octochess {
namespace uci {

class minimalistic_uci_protocol : public gui_interface {
public:
	minimalistic_uci_protocol();

	void thread_entry_point( bool already_got_init );

	virtual void set_engine_interface( engine_interface& );


	virtual void tell_best_move( std::string const& move );
	virtual void tell_info( info const& );

private:
	void init();
	void parse_init( std::string const& line );
	void parse_command( std::string const& line );
	void handle_position( std::string const& params );
	void handle_go( std::string const& params );
	void handle_option( std::string const& args );
	void identify( std::string const& name, std::string const& author );
	void send_options();
private:
	engine_interface* callbacks_;
	bool connected_;
};

}
}

#endif
