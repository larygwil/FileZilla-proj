
#include "octochess_impl.hpp"
#include "info.hpp"
#include "time_calculation.hpp"

#include "../chess.hpp"
#include "../zobrist.hpp"
#include "../seen_positions.hpp"
#include "../book.hpp"
#include "../calc.hpp"
#include "../eval.hpp"
#include "../fen.hpp"
#include "../hash.hpp"
#include "../logger.hpp"
#include "../util.hpp"
#include "../pawn_structure_hash_table.hpp"

#include <sstream>

volatile extern bool do_abort; //magic to kill the calculation

namespace octochess {
namespace uci {

class octochess_uci::impl : public new_best_move_callback, public thread {
public:
	impl( gui_interface_ptr const& p )
		: gui_interface_(p)
		, color_to_play_()
		, last_mate_()
		, half_moves_played_()
		, running_(true)
	{
		spawn();
	}

	 ~impl() {
		 running_ = false;
		 {
			scoped_lock lock(mutex_);
			calc_cond_.signal(lock);
		 }
		 join();
	 }

	virtual void onRun();
	void on_new_best_move( position const& p, color::type c, int depth, int evaluation, uint64_t nodes, pv_entry const* pv );

	void apply_move( move const& m );

public:
	gui_interface_ptr gui_interface_;

	position pos_;
	color::type color_to_play_;
	seen_positions seen_positions_;

	calc_manager calc_manager_;

	time_calculation times_;

	short last_mate_;
	int half_moves_played_;

	bool running_;

	mutex mutex_;
	condition calc_cond_;
};

octochess_uci::octochess_uci( gui_interface_ptr const& p ) 
	: impl_( new impl(p) )
{
	p->set_engine_interface(*this);

	transposition_table.init( conf.memory );
	pawn_hash_table.init( conf.pawn_hash_table_size );
	conf.depth = 40;
	new_game();
}

void octochess_uci::new_game() {
	impl_->color_to_play_ = color::white;
	init_board(impl_->pos_);
	impl_->times_ = time_calculation();
}

void octochess_uci::set_position( std::string const& fen ) {
	init_board(impl_->pos_);
	if( fen.empty() ) {
		impl_->color_to_play_ = color::white;
	} else {
		if( parse_fen_noclock( fen, impl_->pos_, impl_->color_to_play_, 0 ) ) {	
			impl_->seen_positions_.reset_root( get_zobrist_hash( impl_->pos_ ) );
		} else {
			std::cerr << "failed to set position" << std::endl;
		}
	}
}

void octochess_uci::make_moves( std::string const& moves ) {
	bool done = false;
	std::istringstream in( moves );

	std::string str;
	while( !done && in >> str ) {
		move m;
		if( parse_move( impl_->pos_, impl_->color_to_play_, str, m, false ) ) {
			impl_->apply_move( m );
		} else {
			std::cerr << "invalid syntax with moves: " << moves << std::endl;
			done = true;
		}
	}
}

void octochess_uci::calculate( calculate_mode_type mode, position_time const& t ) {
	scoped_lock lock(impl_->mutex_);

	if( mode == calculate_mode::infinite ) {
		impl_->times_.set_infinite_time();
	} else {
		impl_->times_.update( t, impl_->color_to_play_ == color::white, impl_->half_moves_played_ );
	}
	impl_->calc_cond_.signal(lock);
}

void octochess_uci::stop() {
	do_abort = true;
}

void octochess_uci::quit() {
	do_abort = true;
}

void octochess_uci::impl::onRun() {
	//the function initiating all the calculating
	while( running_ ) {
		scoped_lock lock(mutex_);
		calc_cond_.wait(lock);
		if( running_ ) {
			lock.unlock();
			move m;
			int res;

			time start_time;

			bool ret = calc_manager_.calc( pos_, color_to_play_, m, res, times_.time_for_this_move(), half_moves_played_, seen_positions_, last_mate_, *this );
			if( ret ) {
				gui_interface_->tell_best_move( move_to_long_algebraic( m ) );

				lock.lock();

				apply_move( m );

				{
					score base_eval = color_to_play_ ? -pos_.base_eval : pos_.base_eval;
					std::cerr << "  ; Current base evaluation: " << base_eval << " centipawns, forecast " << res << std::endl;
				}

				if( res > result::win_threshold ) {
					last_mate_ = res;
				}

				time stop;
				duration elapsed = stop - start_time;

				std::cerr << "Elapsed: " << elapsed.milliseconds() << " ms" << std::endl;
				times_.after_move_update( elapsed );
			}
		}
	}
}

void octochess_uci::impl::on_new_best_move( position const& p, color::type c, int depth, int evaluation, uint64_t nodes, pv_entry const* pv ) {
	info i;
	i.depth( depth );
	i.node_count( nodes );

	int mate = 0;
	if( evaluation > result::win_threshold ) {
		mate = (result::win - evaluation) / 2;
	} else if( evaluation < result::loss_threshold ) {
		mate = (result::loss - evaluation) / 2;
	}

	if( mate == 0 ) {
		i.score( evaluation );
	} else {
		i.mate_in_n_moves( mate );
	}
	
	//i.time_spent();
	//i.nodes_per_second();

	i.principal_variation( pv_to_string(pv, p, c, true) );

	gui_interface_->tell_info( i );
}


void octochess_uci::impl::apply_move( move const& m )	{
	bool reset_seen = false;
	if( m.piece == pieces::pawn || m.captured_piece ) {
		reset_seen = true;
	}
	::apply_move( pos_, m, color_to_play_ );
	++half_moves_played_;
	color_to_play_ = color_to_play_ == color::white ? color::black : color::white;

	if( !reset_seen ) {
		seen_positions_.push_root( get_zobrist_hash( pos_ ) );
	} else {
		seen_positions_.reset_root( get_zobrist_hash( pos_ ) );
	}	
}

}
}

