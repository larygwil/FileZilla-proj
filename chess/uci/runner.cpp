
#include "minimalistic_uci_protocol.hpp"
#include "octochess_impl.hpp"

#include <memory>

void run_uci() {
	using namespace octochess::uci;

	std::shared_ptr<minimalistic_uci_protocol> gui( new minimalistic_uci_protocol );
	octochess_uci engine( std::static_pointer_cast<gui_interface>(gui) );

	gui->thread_entry_point(); //this blocks until EOF comes to cin
}
