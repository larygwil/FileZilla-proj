
#include "octochess_impl.hpp"
#include "info.hpp"
#include "time_calculation.hpp"

#include "../chess.hpp"
#include "../seen_positions.hpp"
#include "../book.hpp"
#include "../calc.hpp"
#include "../eval.hpp"
#include "../fen.hpp"
#include "../hash.hpp"
#include "../pawn_structure_hash_table.hpp"
#include "../logger.hpp"
#include "../random.hpp"
#include "../util.hpp"
#include "../zobrist.hpp"

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
		, started_from_root_()
		, running_(true)
		, book_( conf.book_dir )
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
	void on_new_best_move( position const& p, color::type c, int depth, int evaluation, uint64_t nodes, duration const& elapsed, pv_entry const* pv );

	void apply_move( move const& m );

	bool do_book_move();

public:
	gui_interface_ptr gui_interface_;

	position pos_;
	color::type color_to_play_;
	seen_positions seen_positions_;

	calc_manager calc_manager_;

	time_calculation times_;

	short last_mate_;
	int half_moves_played_;
	bool started_from_root_;

	bool running_;

	mutex mutex_;
	condition calc_cond_;

	book book_;
};

octochess_uci::octochess_uci( gui_interface_ptr const& p ) 
	: impl_( new impl(p) )
{
	p->set_engine_interface(*this);

	transposition_table.init( conf.memory );
	pawn_hash_table.init( conf.pawn_hash_table_size );
	if( conf.depth == -1 ) {
		conf.depth = MAX_DEPTH;
	}
	new_game();
}

void octochess_uci::new_game() {
	impl_->color_to_play_ = color::white;
	init_board(impl_->pos_);
	impl_->seen_positions_.reset_root( get_zobrist_hash( impl_->pos_ ) ); impl_->times_ = time_calculation();
	impl_->half_moves_played_ = 0;
	impl_->started_from_root_ = true;
	impl_->last_mate_ = 0;
}

void octochess_uci::set_position( std::string const& fen ) {
	init_board(impl_->pos_);
	bool success = false;
	if( fen.empty() ) {
		impl_->color_to_play_ = color::white;
		impl_->started_from_root_ = true;
		success = true;
	}
	else {
		if( parse_fen_noclock( fen, impl_->pos_, impl_->color_to_play_, 0 ) ) {	
			impl_->started_from_root_ = false;
			success = true;
		}
		else {
			std::cerr << "failed to set position" << std::endl;
		}
	}

	if( success ) {
		impl_->seen_positions_.reset_root( get_zobrist_hash( impl_->pos_ ) );
		impl_->half_moves_played_ = 0;
		impl_->last_mate_ = 0;
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
		impl_->calc_cond_.signal(lock);
	}
	else {
		impl_->times_.update( t, impl_->color_to_play_ == color::white, impl_->half_moves_played_ );
		bool got_book_move = impl_->do_book_move();
		if( !got_book_move ) {
			impl_->calc_cond_.signal(lock);
		}
	}
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

			timestamp start_time;

			calc_result result = calc_manager_.calc( pos_, color_to_play_, times_.time_for_this_move(), half_moves_played_, seen_positions_, last_mate_, *this );
			if( !result.best_move.empty() ) {
				gui_interface_->tell_best_move( move_to_long_algebraic( result.best_move ) );

				lock.lock();

				apply_move( result.best_move );

				{
					score base_eval = color_to_play_ ? -pos_.base_eval : pos_.base_eval;
					std::cerr << "  ; Current base evaluation: " << base_eval << " centipawns, forecast " << result.forecast << std::endl;
				}

				if( result.forecast > result::win_threshold ) {
					last_mate_ = result.forecast;
				}

				timestamp stop;
				duration elapsed = stop - start_time;

				std::cerr << "Elapsed: " << elapsed.milliseconds() << " ms" << std::endl;
				times_.after_move_update( elapsed );
			}
		}
	}
}

void octochess_uci::impl::on_new_best_move( position const& p, color::type c, int depth, int evaluation, uint64_t nodes, duration const& elapsed, pv_entry const* pv ) {
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
	
	i.time_spent( elapsed );
	if( !elapsed.empty() ) {
		i.nodes_per_second( elapsed.get_items_per_second(nodes) );
	}

	i.principal_variation( pv_to_string(pv, p, c, true) );

	gui_interface_->tell_info( i );
}


void octochess_uci::impl::apply_move( move const& m )
{
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


bool octochess_uci::impl::do_book_move() {
	bool ret = false;

	if( conf.use_book && book_.is_open() && half_moves_played_ < 30 && started_from_root_ ) {
		std::vector<book_entry> moves = book_.get_entries( pos_, color_to_play_ );
		if( !moves.empty() ) {
			ret = true;

			short best = moves.front().folded_forecast;
			int count_best = 1;
			for( std::vector<book_entry>::const_iterator it = moves.begin() + 1; it != moves.end(); ++it ) {
				if( it->folded_forecast > -33 && it->folded_forecast + 25 >= best && count_best < 3 ) {
					++count_best;
				}
			}

			book_entry best_move = moves[get_random_unsigned_long_long() % count_best];
			gui_interface_->tell_best_move( move_to_long_algebraic( best_move.m ) );
			apply_move( best_move.m );
		}
	}

	return ret;
}


}
}

