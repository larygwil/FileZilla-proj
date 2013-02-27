
#ifndef OCTOCHESS_UCI_OCTOCHESS_IMPL_HEADER
#define OCTOCHESS_UCI_OCTOCHESS_IMPL_HEADER

#include "interface.hpp"

#include "../config.hpp"

#include <vector>

namespace octochess {
namespace uci {

class octochess_uci : engine_interface {
public:
	octochess_uci( gui_interface_ptr const& );
	//callbacks
	virtual void new_game();
	virtual void set_position( std::string const& fen );
	virtual void make_moves( std::string const& list_of_moves );
	virtual void calculate( calculate_mode_type, position_time const&, int depth, bool ponder );
	virtual void stop();
	virtual void quit();

	//generic info
	virtual std::string name() const { return conf.program_name(); }
	virtual std::string author() const { return "Tim Kosse"; }

	// options
	virtual uint64_t get_hash_size() const; // In MiB
	virtual uint64_t get_min_hash_size() const; // In MiB
	virtual void set_hash_size( uint64_t mb );
	virtual unsigned int get_threads() const;
	virtual unsigned int get_max_threads() const;
	virtual void set_threads( unsigned int threads );
	virtual bool use_book() const;
	virtual void use_book( bool use );
	virtual void set_multipv( unsigned int multipv );

private:
	virtual void make_moves( std::vector<std::string> const& list_of_moves );
private:
	class impl;
	std::shared_ptr<impl> impl_;
};

}
}

#endif
