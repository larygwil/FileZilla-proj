
#include "octochess_impl.hpp"
#include "info.hpp"
#include "uci_time_calculation.hpp"

#include "../chess.hpp"
#include "../simple_book.hpp"
#include "../calc.hpp"
#include "../eval.hpp"
#include "../fen.hpp"
#include "../hash.hpp"
#include "../pawn_structure_hash_table.hpp"
#include "../state_base.hpp"
#include "../util/logger.hpp"
#include "../util/mutex.hpp"
#include "../util/string.hpp"
#include "../util/thread.hpp"
#include "../util.hpp"

#include <sstream>
#include <set>

namespace octochess {
namespace uci {

class octochess_uci::impl : public new_best_move_callback_base, public thread, public state_base {
public:
	impl( context& ctx, gui_interface_ptr const& p )
		: state_base(ctx)
		, gui_interface_(p)
		, calc_manager_(ctx)
		, depth_(-1)
		, wait_for_stop_(false)
		, ponder_()
	{
	}

	~impl() {
		calc_manager_.abort();
		join();
	}

	virtual void onRun();
	void on_new_best_move( unsigned int multipv, position const& p, int depth, int selective_depth, int evaluation, uint64_t nodes, duration const& elapsed, move const* pv ) override;

	bool do_book_move();
	bool pick_pv_move();

public:
	gui_interface_ptr gui_interface_;

	calc_manager calc_manager_;

	mutex mutex_;

	int depth_;
	bool wait_for_stop_;
	bool ponder_;

	calc_result result_;
};

octochess_uci::octochess_uci( context& ctx,  gui_interface_ptr const& p ) 
	: impl_( new impl(ctx, p) )
{
	p->set_engine_interface(*this);

	ctx.pawn_tt_.init( ctx.conf_.pawn_hash_table_size() );
	new_game();
}

void octochess_uci::new_game() {
	impl_->calc_manager_.abort();
	impl_->join();
	impl_->reset();
	impl_->times_ = time_calculation();
}

void octochess_uci::set_position( std::string const& fen ) {
	impl_->calc_manager_.abort();
	impl_->join();
	impl_->reset();

	if( !fen.empty() ) {
		position p;
		if( parse_fen( impl_->ctx_.conf_, fen, p, 0 ) ) {
			impl_->reset(p);
		}
		else {
			std::cerr << "failed to set position" << std::endl;
		}
	}

	impl_->ctx_.tt_.init( impl_->ctx_.conf_.memory );
}

void octochess_uci::make_moves( std::string const& moves ) {
	make_moves( tokenize( moves ) );
}

void octochess_uci::make_moves( std::vector<std::string> const& moves )
{
	bool done = false;
	for( std::vector<std::string>::const_iterator it = moves.begin(); !done && it != moves.end(); ++it ) {
		move m;
		std::string error;
		if( parse_move( impl_->p(), *it, m, error ) ) {
			impl_->apply( m );
		} else {
			std::cerr << error << ": " << *it << std::endl;
			done = true;
		}
	}
}

void octochess_uci::calculate( timestamp const& start, calculate_mode_type mode, position_time const& t, int depth, bool ponder, std::string const& searchmoves )
{
	impl_->ctx_.tt_.init( impl_->ctx_.conf_.memory );

	impl_->calc_manager_.abort();
	impl_->join();

	scoped_lock lock(impl_->mutex_);

	impl_->result_.best_move.clear();
	impl_->calc_manager_.clear_abort();
	impl_->times_.set_start(start);

	if( mode == calculate_mode::ponderhit ) {
		impl_->ponder_ = false;
		impl_->spawn();
	}
	else {
		impl_->depth_ = depth;
		impl_->wait_for_stop_ = mode == calculate_mode::infinite;
		impl_->ponder_ = ponder;
		impl_->searchmoves_.clear();

		for( auto const& ms : tokenize(searchmoves) ) {
			std::string error;

			move m;
			if( parse_move( impl_->p(), ms, m, error ) ) {
				impl_->searchmoves_.insert( m );
			}
		}

		impl_->times_.set_moves_to_go( t.moves_to_go(), true );
		impl_->times_.set_movetime( t.movetime() );
		for( int i = 0; i < 2; ++i ) {
			impl_->times_.set_remaining( i, t.time_left(i), impl_->clock() );
			impl_->times_.set_increment( i, t.increment(i) );
		}

		std::pair<duration, duration> times = impl_->times_.update( impl_->p().black(), impl_->clock(), impl_->recapture() );
		if( ponder || times.first.is_infinity() || impl_->wait_for_stop_ || !impl_->searchmoves_.empty() ) {
			impl_->spawn();
		}
		else {
			if( !impl_->do_book_move() ) {
				if( !impl_->pick_pv_move() ) {
					impl_->spawn();
				}
			}
		}
	}
}

void octochess_uci::stop()
{
	impl_->calc_manager_.abort();
	impl_->join();

	scoped_lock lock(impl_->mutex_);

	if( !impl_->result_.best_move.empty() ) {
		impl_->gui_interface_->tell_best_move(
			move_to_long_algebraic( impl_->ctx_.conf_, impl_->p(), impl_->result_.best_move ),
			move_to_long_algebraic( impl_->ctx_.conf_, impl_->p(), impl_->result_.ponder_move ) );
		impl_->result_.best_move.clear();
	}
}

void octochess_uci::quit() {
	impl_->calc_manager_.abort();
}

void octochess_uci::impl::onRun() {
	//the function initiating all the calculating
	if( ponder_ ) {
		std::cerr << "Pondering..." << std::endl;
		calc_result result = calc_manager_.calc( p(), -1, times_.start(), duration::infinity(), duration::infinity()
			, clock(), seen(), *this, searchmoves_ );

		scoped_lock lock(mutex_);

		if( !result.best_move.empty() ) {
			result_ = result;
		}
	}
	else {
		std::pair<duration, duration> times = times_.update( p().black(), clock(), recapture() );
		calc_result result = calc_manager_.calc( p(), depth_, times_.start(), times.first, times.second
			, clock(), seen(), *this, searchmoves_ );

		scoped_lock lock(mutex_);

		if( !result.best_move.empty() ) {

			if( !wait_for_stop_ ) {
				gui_interface_->tell_best_move( move_to_long_algebraic( ctx_.conf_, p(), result.best_move ), move_to_long_algebraic( ctx_.conf_, p(), result.ponder_move ) );
				result_.best_move.clear();
			}
			else {
				result_ = result;
			}
			times_.update_after_move( result.used_extra_time, true, true );
		}
	}
}

void octochess_uci::impl::on_new_best_move( unsigned int multipv, position const& p, int depth, int selective_depth, int evaluation, uint64_t nodes, duration const& elapsed, move const* pv ) {
	info i;
	i.depth( depth );
	i.selective_depth( selective_depth );
	i.node_count( nodes );
	i.multipv( multipv );

	int mate = 0;
	if( evaluation > result::win_threshold ) {
		mate = (result::win - evaluation + 1) / 2;
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

	i.principal_variation( pv_to_string(ctx_.conf_, pv, p, true) );

	gui_interface_->tell_info( i );

	pv_move_picker_.update_pv( p, pv );
}


bool octochess_uci::impl::do_book_move() {
	move m = get_book_move();
	if( !m.empty() ) {
		gui_interface_->tell_best_move( move_to_long_algebraic( ctx_.conf_, p(), m ), "" );
	}

	return !m.empty();
}


bool octochess_uci::impl::pick_pv_move()
{
	bool ret = false;
	std::pair<move,move> m = pv_move_picker_.can_use_move_from_pv( p() );
	if( !m.first.empty() ) {
		ret = true;

		// From the UCI specs:
		// * bestmove[ponder]
		//	the engine has stopped searching and found the move  best in this position.
		//  [..]
		//  Directly before that the engine should send a final "info" command with the final search information,
		//  then the GUI has the complete statistics about the last search.
		info i;
		i.depth(1);
		i.selective_depth(1);
		i.node_count(1);
		i.multipv(1);
		i.time_spent( duration() );

		{
			move pv[3];
			short ev = get_pv_from_tt(ctx_.tt_, pv, p(), 2);
			if( ev != result::loss ) {
				i.score( ev );
			}
			if( !pv[0].empty() ) {
				i.principal_variation( pv_to_string( ctx_.conf_, pv, p(), true ) );
			}
		}

		gui_interface_->tell_info(i);

		gui_interface_->tell_best_move( move_to_long_algebraic( ctx_.conf_, p(), m.first ), move_to_long_algebraic( ctx_.conf_, p(), m.second ) );
	}

	return ret;
}


uint64_t octochess_uci::get_hash_size() const
{
	return impl_->ctx_.conf_.memory;
}


uint64_t octochess_uci::get_min_hash_size() const
{
	return 4ull;
}


void octochess_uci::set_hash_size( uint64_t mb )
{
	if( mb < 4 ) {
		mb = 4;
	}
	impl_->ctx_.conf_.memory = static_cast<unsigned int>(mb);
}


unsigned int octochess_uci::get_threads() const
{
	return impl_->ctx_.conf_.thread_count;
}


unsigned int octochess_uci::get_max_threads() const
{
	return get_cpu_count();
}


void octochess_uci::set_threads( unsigned int threads )
{
	if( !threads ) {
		threads = 1;
	}
	if( threads > get_cpu_count() ) {
		threads = get_cpu_count();
	}
	impl_->ctx_.conf_.thread_count = threads;
}


bool octochess_uci::use_book() const
{
	return impl_->book_.is_open();
}


void octochess_uci::use_book( bool use )
{
	if( use ) {
		impl_->book_.open( impl_->ctx_.conf_.self_dir );
	}
	else {
		impl_->book_.close();
	}
}


void octochess_uci::set_multipv( unsigned int multipv )
{
	impl_->calc_manager_.set_multipv( multipv );
}


bool octochess_uci::is_move( std::string const& ms )
{
	bool ret = false;
	std::string error;

	move m;
	if( parse_move( impl_->p(), ms, m, error ) ) {
		ret = true;
	}

	return ret;
}


void octochess_uci::fischer_random( bool frc )
{
	impl_->ctx_.conf_.fischer_random = frc;
}


std::string octochess_uci::name() const
{
	return impl_->ctx_.conf_.program_name();
}

}
}

