#include "uci_time_calculation.hpp"

#include "../chess.hpp"

namespace octochess {
namespace uci {

position_time::position_time()
	: moves_to_go_()
{
	left_[0] = left_[1] = duration::infinity();
}

}
}