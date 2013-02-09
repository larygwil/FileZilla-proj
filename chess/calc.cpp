#include "assert.hpp"
#include "calc.hpp"
#include "config.hpp"
#include "eval.hpp"
#include "eval_values.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "moves.hpp"
#include "phased_move_generator.hpp"
#include "random.hpp"
#include "see.hpp"
#include "statistics.hpp"
#include "tables.hpp"
#include "util/mutex.hpp"
#include "util/thread.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <map>
#include <sstream>
#include <string>
#include <vector>

int const check_extension = 6;
int const pawn_push_extension = 6;
int const recapture_extension = 6;
int const cutoff = depth_factor + MAX_QDEPTH + 1;

int const lmr_min_depth = cutoff + depth_factor * 2;

short const razor_pruning[] = { 220, 250, 290 };

#define USE_FUTILITY 1
short const futility_pruning[] = { 110, 130, 170, 210 };

#define NULL_MOVE_REDUCTION 3

int const NULLMOVE_VERIFICATION_DEPTH = cutoff + depth_factor * 5;

int const delta_pruning = 50;

volatile bool do_abort = false;

short const ASPIRATION = 40;

def_new_best_move_callback default_new_best_move_callback;
null_new_best_move_callback null_new_best_move_cb;

void sort_moves( move_info* begin, move_info* end, position const& p )
{
	for( move_info* it = begin; it != end; ++it ) {
		it->sort = evaluate_move( p, it->m );
		pieces::type captured_piece = p.get_captured_piece( it->m );
		if( captured_piece != pieces::none ) {
			pieces::type piece = p.get_piece( it->m.source() );
			it->sort += eval_values::material_values[ captured_piece ].mg() * 1000000 - eval_values::material_values[ piece ].mg();
		}
	}
	std::stable_sort( begin, end, moveSort );
}


short quiescence_search( int ply, int depth, context& ctx, position const& p, uint64_t hash, check_map const& check, short alpha, short beta, short full_eval = result::win )
{
#if VERIFY_HASH
	if( get_zobrist_hash(p) != hash ) {
		std::cerr << "FAIL HASH!" << std::endl;
	}
#endif
	ASSERT( p.verify() );

	if( do_abort ) {
		return result::loss;
	}

#ifdef USE_STATISTICS
	++stats.quiescence_nodes;
#endif

	if( !p.bitboards[color::white].b[bb_type::pawns] && !p.bitboards[color::black].b[bb_type::pawns] ) {
		if( p.material[color::white].eg() + p.material[color::black].eg() <= eval_values::insufficient_material_threshold ) {
			return result::draw;
		}
	}

	if( !depth ) {
		return result::draw; //beta;
	}

	bool pv_node = alpha + 1 != beta;

	bool do_checks = depth >= MAX_QDEPTH;

	int tt_depth = do_checks ? 2 : 1;

	short eval;
	move tt_move;
	score_type::type t = transposition_table.lookup( hash, tt_depth, ply, alpha, beta, eval, tt_move, full_eval );
	ASSERT( full_eval == result::win || full_eval == evaluate_full( p ) );

	if ( !pv_node && t != score_type::none ) {
		return eval;
	}

#if 0
	full_eval = evaluate_full( p, p.self(), current_evaluation );
	short diff = std::abs( full_eval - current_evaluation );
	if( diff > LAZY_EVAL ) {
		std::cerr << "Bug in lazy evaluation, full evaluation differs too much from basic evaluation:" << std::endl;
		std::cerr << "Position:   " << position_to_fen_noclock( p, c ) << std::endl;
		std::cerr << "Current:    " << current_evaluation << std::endl;
		std::cerr << "Full:       " << full_eval << std::endl;
		std::cerr << "Difference: " << full_eval - current_evaluation << std::endl;
		abort();
	}
#endif

	short old_alpha = alpha;

	if( !check.check ) {
		if( full_eval == result::win ) {
			full_eval = evaluate_full( p );
		}
		if( full_eval > alpha ) {
			if( full_eval >= beta ) {
				if( !do_abort && t == score_type::none && tt_move.empty() ) {
					transposition_table.store( hash, tt_depth, ply, full_eval, alpha, beta, tt_move, ctx.clock, full_eval );
				}
				return full_eval;
			}
			alpha = full_eval;
		}
	}

	qsearch_move_generator gen( ctx, p, check, pv_node, do_checks );

	if( check.check || (!tt_move.empty() && p.get_piece( tt_move.target() ) != pieces::none ) ) {
		gen.hash_move = tt_move;
	}

	move best_move;
	short best_value = check.check ? static_cast<short>(result::loss) : full_eval;

	move_info const* it;
	while( (it = gen.next()) ) {

		pieces::type piece = p.get_piece( it->m.source() );
		pieces::type captured_piece = p.get_captured_piece( it->m );
		
		position new_pos(p);
		apply_move( new_pos, it->m );
		check_map new_check( new_pos );
		
		if( captured_piece == pieces::none && !check.check && !new_check.check ) {
			continue;
		}

		// Delta pruning
		if( !pv_node && !check.check && !new_check.check && (piece != pieces::pawn || (it->m.target() >= 16 && it->m.target() < 48 ) ) ) {
			short new_value = full_eval + eval_values::material_values[captured_piece].mg() + delta_pruning;
			if( new_value <= alpha ) {
				if( new_value > best_value ) {
					best_value = new_value;
				}
				continue;
			}
		}

		short value;

		uint64_t new_hash = update_zobrist_hash( p, hash, it->m );
		value = -quiescence_search( ply + 1, depth - 1, ctx, new_pos, new_hash, new_check, -beta, -alpha );

		if( value > best_value ) {
			best_value = value;

			if( value > alpha ) {
				best_move = it->m;
				
				if( value >= beta ) {
					break;
				}
				else {
					alpha = value;
				}
			}
		}
	}

	if( best_value == result::loss && check.check ) {
		return result::loss + ply;
	}

	if( !do_abort ) {
		transposition_table.store( hash, tt_depth, ply, best_value, old_alpha, beta, best_move, ctx.clock, full_eval );
	}
	return best_value;
}


short step( int depth, int ply, context& ctx, position& p, uint64_t hash, check_map const& check, short alpha, short beta, bool last_was_null, short full_eval, unsigned char last_ply_was_capture )
{
#if VERIFY_HASH
	if( get_zobrist_hash(p) != hash ) {
		std::cerr << "FAIL HASH!" << std::endl;
	}
#endif
	ASSERT( p.verify() );

	if( depth < cutoff || ply >= MAX_DEPTH ) {
		return quiescence_search( ply, MAX_QDEPTH, ctx, p, hash, check, alpha, beta );
	}

	if( do_abort ) {
		return result::loss;
	}

#ifdef USE_STATISTICS
	stats.node( ply );
#endif

	if( !p.bitboards[color::white].b[bb_type::pawns] && !p.bitboards[color::black].b[bb_type::pawns] ) {
		if( p.material[color::white].eg() + p.material[color::black].eg() <= eval_values::insufficient_material_threshold ) {
			return result::draw;
		}
	}

	bool const pv_node = alpha + 1 != beta;
	short const old_alpha = alpha;

	// Mate distance pruning
	beta = std::min( static_cast<short>(result::win - ply - 1), beta );
	alpha = std::max( static_cast<short>(result::loss + ply), alpha );
	if( alpha >= beta ) {
		return alpha;
	}

	// Transposition table lookup and cutoff
	move tt_move;
	score_type::type t;
	{
		short eval;
		t = transposition_table.lookup( hash, depth, ply, alpha, beta, eval, tt_move, full_eval );
		if( t != score_type::none ) {
			if( t == score_type::exact || !pv_node ) {
				return eval;
			}
		}
	}

	int plies_remaining = (depth - cutoff) / depth_factor;

	if( !pv_node && !check.check /*&& plies_remaining < 4*/ && full_eval == result::win ) {
		full_eval = evaluate_full( p );
		if( tt_move.empty() && t == score_type::none ) {
			transposition_table.store( hash, 0, ply, 0, 0, 0, tt_move, ctx.clock, full_eval );
		}
	}

	if( !pv_node && !check.check && plies_remaining < static_cast<int>(sizeof(razor_pruning)/sizeof(short)) && full_eval + razor_pruning[plies_remaining] < beta &&
		   tt_move.empty() && beta < result::win_threshold && beta > result::loss_threshold &&
		   !(p.bitboards[p.self()].b[bb_type::pawns] & (p.white() ? 0x00ff000000000000ull : 0x000000000000ff00ull)) )
	{
		short new_beta = beta - razor_pruning[plies_remaining];
		short value = quiescence_search( ply, MAX_QDEPTH, ctx, p, hash, check, new_beta - 1, new_beta, full_eval );
		if( value < new_beta ) {
			return value;
		}
	}

#if NULL_MOVE_REDUCTION > 0
	if( !pv_node && !check.check &&
		full_eval >= beta && !last_was_null &&
		beta > result::loss_threshold && beta < result::win_threshold &&
		depth >= (cutoff + depth_factor) && p.material[p.self()].mg() )
	{
		short new_depth = depth - (NULL_MOVE_REDUCTION + 1) * depth_factor;
		short value;

		{
			null_move_block seen_block( ctx.seen, ply );

			unsigned char old_enpassant = p.do_null_move();
			check_map new_check( p );
			uint64_t new_hash = ~(hash ^ get_enpassant_hash( old_enpassant ) );
			value = -step( new_depth, ply + 1, ctx, p, new_hash, new_check, -beta, -beta + 1, true );
			p.do_null_move();
			p.can_en_passant = old_enpassant;
		}

		if( value >= beta ) {
			if( value >= result::win_threshold ) {
				value = beta;
			}

			if( depth > NULLMOVE_VERIFICATION_DEPTH ) {
				// Verification search.
				// Helps against zugzwang and some other strange issues
				short research_value = step( new_depth, ply, ctx, p, hash, check, alpha, beta, true, full_eval, last_ply_was_capture );
				if( research_value >= beta ) {
					return value;
				}
			}
			else {
				return value;
			}
		}
	}
#endif

	if( tt_move.empty() && depth > ( depth_factor * 4 + cutoff) ) {

		step( depth - 2 * depth_factor, ply, ctx, p, hash, check, alpha, beta, true, full_eval, last_ply_was_capture );

		short eval;
		transposition_table.lookup( hash, depth, ply, alpha, beta, eval, tt_move, full_eval );
	}

	short best_value = result::loss;
	move best_move;

	unsigned int processed_moves = 0;
	
	move_generator gen( ctx, ctx.killers[p.self()][ply], p, check );
	gen.hash_move = tt_move;
	move_info const* it;

	while( (it = gen.next()) ) {
		++processed_moves;

		short value;
		uint64_t new_hash = update_zobrist_hash( p, hash, it->m );

		pieces::type piece = p.get_piece( it->m.source() );
		pieces::type captured_piece = p.get_captured_piece( it->m );

		if( ctx.seen.is_two_fold( new_hash, ply ) ) {
			value = result::draw;
		}
		else {
			position new_pos(p);
			apply_move( new_pos, it->m );

			ctx.seen.set( new_hash, ply );

			check_map new_check( new_pos );

			bool extended = false;

			int new_depth = depth - depth_factor;

			// Check extension
			if( new_check.check ) {
				new_depth += check_extension;
				extended = true;
			}

			bool dangerous_pawn_move = false;

			// Pawn push extension
			if( piece == pieces::pawn ) {
				// Pawn promoting or moving to 7th rank
				if( it->m.target() < 16 || it->m.target() >= 48 ) {
					dangerous_pawn_move = true;
					if( !extended && pv_node ) {
						new_depth += pawn_push_extension;
						extended = true;
					}
				}
				// Pushing a passed pawn
				else if( !(passed_pawns[p.self()][it->m.target()] & p.bitboards[p.other()].b[bb_type::pawns] ) ) {
					dangerous_pawn_move = true;
					if( !extended && pv_node ) {
						new_depth += pawn_push_extension;
						extended = true;
					}
				}
			}

			// Recapture extension
			if( !extended && pv_node && captured_piece != pieces::none && it->m.target() == last_ply_was_capture && see(p, it->m) >= 0 ) {
				new_depth += recapture_extension;
				extended = true;
			}
			unsigned char new_capture = 64;
			if( captured_piece != pieces::none ) {
				new_capture = it->m.target();
			}

			// Open question: What's the exact reason for always searching exactly the first move full width?
			// Why not always use PVS, or at least in those cases where root alpha isn't result::loss?
			// Why not use full width unless alpha > old_alpha?
			if( processed_moves || !pv_node ) {

#if USE_FUTILITY
				// Futility pruning
				if( !extended && !pv_node && gen.get_phase() == phases::noncapture && !check.check &&
					!dangerous_pawn_move &&
					( best_value == result::loss || best_value < result::loss_threshold ) )
				{
					int plies_remaining = (depth - cutoff) / depth_factor;
					if( plies_remaining < static_cast<int>(sizeof(futility_pruning)/sizeof(short))) {
						value = full_eval + futility_pruning[plies_remaining];
						if( value <= alpha ) {
							continue;
						}
					}

					if( new_depth < cutoff + depth_factor && see( p, it->m ) < 0 ) {
						continue;
					}
				}

#endif

				bool search_full;
				if( !extended && processed_moves >= (pv_node ? 5u : 3u) && gen.get_phase() >= phases::noncapture &&
					!check.check && depth >= lmr_min_depth )
				{
					int red = pv_node ? (depth_factor) : (depth_factor * 2);

					red += (processed_moves - (pv_node?3:3)) / 5;
					int lmr_depth = new_depth - red;
					value = -step(lmr_depth, ply + 1, ctx, new_pos, new_hash, new_check, -alpha-1, -alpha, false );
				
					search_full = value > alpha;
				}
				else {
					search_full = true;
				}
				
				if( search_full ) {
					value = -step( new_depth, ply + 1, ctx, new_pos, new_hash, new_check, -alpha-1, -alpha, false, result::win, new_capture );
				}
			}

			if( pv_node && (!processed_moves || (value > alpha && value < beta) ) ) {
				value = -step( new_depth, ply + 1, ctx, new_pos, new_hash, new_check, -beta, -alpha, false, result::win, new_capture );
			}
		}
		if( value > best_value ) {
			best_value = value;

			if( value > alpha ) {
				best_move = it->m;

				if( value >= beta ) {		
					if( !captured_piece ) {
						ctx.killers[p.self()][ply].add_killer( it->m );
						gen.update_history();
					}
					break;
				}
				alpha = value;
			}
		}
	}

	if( !processed_moves ) {
		if( check.check ) {
			return result::loss + ply;
		}
		else {
			return result::draw;
		}
	}

	if( best_value == result::loss ) {
		best_value = old_alpha;
	}

	if( !do_abort ) {
		transposition_table.store( hash, depth, ply, best_value, old_alpha, beta, best_move, ctx.clock, full_eval );
	}

	return best_value;
}

namespace {
class processing_thread : public thread
{
public:
	enum state_type {
		idle,
		busy,
		pending_results,
		waiting,
		quitting
	};

	processing_thread( mutex& mtx, condition& cond )
		: mutex_(mtx), cond_(cond), clock_(), state_(idle)
	{
		spawn();
	}


	virtual ~processing_thread()
	{
		join();
	}


	void quit( scoped_lock& l )
	{
		state_ = quitting;
		waiting_on_work_.signal( l );
	}


	void wait_idle( scoped_lock& l )
	{
		if( state_ == idle ) {
			return;
		}
		else if( state_ == pending_results ) {
			state_ = idle;
		}
		else if( state_ == busy ) {
			state_ = waiting;
			waiting_on_work_.signal( l );
		}
	}

	// Call locked
	bool got_results() {
		return state_ == pending_results;
	}

	bool is_idle() {
		return state_ == idle;
	}

	// Call locked
	short result() {
		if( state_ != pending_results ) {
			std::cerr << "Calling result in wrong state." << std::endl;
			abort();
		}
		state_ = idle;
		return result_;
	}

	void process( scoped_lock& l, position const& p, uint64_t hash, move_info const& m, short max_depth, short quiescence_depth, short alpha_at_prev_depth, short alpha, short beta, int clock, seen_positions const& seen )
	{
		p_ = p;
		hash_ = hash;
		m_ = m;
		max_depth_ = max_depth;
		quiescence_depth_ = quiescence_depth;
		alpha_at_prev_depth_ = alpha_at_prev_depth;
		alpha_ = alpha;
		beta_ = beta;
		clock_ = clock;
		
		seen_ = seen;

		state_ = busy;

		waiting_on_work_.signal( l );
	}

	move_info get_move() const { return m_; }

	void reduce_history() {
		ctx_.history_.reduce();
	}

	virtual void onRun();
private:
	short processWork();

	mutex& mutex_;
	condition& cond_;

	condition waiting_on_work_;

	position p_;
	uint64_t hash_;
	move_info m_;
	short max_depth_;
	short quiescence_depth_;
	short alpha_at_prev_depth_;
	short alpha_;
	short beta_;

	short result_;
	int clock_;

	seen_positions seen_;

	context ctx_;

	state_type state_;
};
}


short processing_thread::processWork()
{
	position new_pos = p_;
	apply_move( new_pos, m_.m );
	uint64_t hash = update_zobrist_hash( p_, hash_, m_.m );

	ctx_.clock = clock_ % 256;
	ctx_.seen = seen_;
	ctx_.move_ptr = ctx_.moves;

	if( ctx_.seen.is_two_fold( hash, 1 ) ) {
		return result::draw;
	}

	ctx_.seen.push_root( hash );

	check_map check( new_pos );

	// Search using aspiration window:
	short value;
	if( alpha_ == result::loss && alpha_at_prev_depth_ != result::loss ) {
		// Windows headers unfortunately create some defines called max and min :(
		short alpha = std::max( alpha_, static_cast<short>(alpha_at_prev_depth_ - ASPIRATION) );
		short beta = std::min( beta_, static_cast<short>(alpha_at_prev_depth_ + ASPIRATION) );

		if( alpha < beta ) {
			value = -step( max_depth_ * depth_factor + MAX_QDEPTH + 1, 1, ctx_, new_pos, hash, check, -beta, -alpha, false );
			if( value > alpha && value < beta ) {
				// Aspiration search found something sensible
				return value;
			}
		}
	}

	if( alpha_ != result::loss ) {
		value = -step( max_depth_ * depth_factor + MAX_QDEPTH + 1, 1, ctx_, new_pos, hash, check, -alpha_-1, -alpha_, false );
		if( value > alpha_ ) {
			value = -step( max_depth_ * depth_factor + MAX_QDEPTH + 1, 1, ctx_, new_pos, hash, check, -beta_, -alpha_, false );
		}
	}
	else {
		value = -step( max_depth_ * depth_factor + MAX_QDEPTH + 1, 1, ctx_, new_pos, hash, check, -beta_, -alpha_, false );
	}

	return value;
}


void processing_thread::onRun()
{
	scoped_lock l(mutex_);

	while( true ) {

		while( state_ == idle || state_ == pending_results ) {
			waiting_on_work_.wait( l );
		}

		if( state_ == quitting ) {
			return;
		}

		if( state_ == busy ) {
			l.unlock();
			short result = processWork();
			l.lock();
			result_ = result;
			if( state_ == busy ) {
				state_ = pending_results;
			}
		}
		
		if( state_ == waiting ) {
			state_ = idle;
		}

		cond_.signal( l );
	}
}

struct move_data {
	enum state {
		waiting,
		busy,
		done
	};

	move_data() : s(waiting) {}

	state s;
	move_info m;
	move pv[MAX_DEPTH];
};

typedef std::vector<move_data> sorted_moves;

void def_new_best_move_callback::on_new_best_move( position const& p, int depth, int selective_depth , int evaluation, uint64_t nodes, duration const& /*elapsed*/, move const* pv )
{
	ss_.str( std::string() );
	ss_ << "Best: " << std::setw(2) << depth << " " << std::setw(2) << selective_depth << " " << std::setw(7) << evaluation << " " << std::setw(10) << nodes << " " << std::setw(0) << pv_to_string( pv, p ) << std::endl;
	std::cerr << ss_.str();
}

class calc_manager::impl
{
public:
	impl()
	{
		update_threads();
	}

	~impl() {
		{
			scoped_lock l( mtx_ );

			for( std::vector<processing_thread*>::iterator it = threads_.begin(); it != threads_.end(); ++it ) {
				(*it)->quit( l );
			}
		}

		for( std::vector<processing_thread*>::iterator it = threads_.begin(); it != threads_.end(); ++it ) {
			delete *it;
		}
	}

	void update_threads() {
		while( threads_.size() < conf.thread_count ) {
			threads_.push_back( new processing_thread( mtx_, cond_ ) );
		}
		while( threads_.size() > conf.thread_count ) {
			std::vector<processing_thread*>::iterator it = --threads_.end();

			{
				scoped_lock l( mtx_ );
				(*it)->quit( l );
			}

			delete *it;
			threads_.erase( it );
		}
	}

	void reduce_histories()
	{
		for( std::vector<processing_thread*>::iterator it = threads_.begin(); it != threads_.end(); ++it ) {
			(*it)->reduce_history();
		}
	}

	int get_max_depth( int depth ) const
	{
		if( depth <= 0 ) {
			return conf.max_search_depth();
		}
		else {
			return std::min( depth, conf.max_search_depth() );
		}
	}

	mutex mtx_;
	condition cond_;
	std::vector<processing_thread*> threads_;
};


calc_manager::calc_manager()
	: impl_( new impl() )
{
	
}


calc_manager::~calc_manager()
{
	delete impl_;
}


calc_result calc_manager::calc( position& p, int max_depth, duration const& move_time_limit, duration const& deadline, int clock, seen_positions& seen
		  , short last_mate
		  , new_best_move_callback_base& new_best_cb )
{
	ASSERT( move_time_limit <= deadline );

	max_depth = impl_->get_max_depth( max_depth );

	impl_->update_threads();
	impl_->reduce_histories();

	calc_result result;

	bool ponder = false;
	if( !move_time_limit.is_infinity() ) {
		if( !deadline.is_infinity() ) {
			std::cerr << "Time limit is " << move_time_limit.milliseconds() << " ms with deadline of " << deadline.milliseconds() << " ms" << std::endl;
		}
		else {
			std::cerr << "Time limit is " << move_time_limit.milliseconds() << " ms without deadline" << std::endl;
		}
	}
	else {
		ponder = true;
	}

	check_map check( p );

	move_info moves[200];
	move_info* pm = moves;

	calculate_moves( p, pm, check );
	sort_moves( moves, pm, p );

	duration time_limit = move_time_limit;

	if( moves == pm ) {
		if( check.check ) {
			if( p.white() ) {
				std::cerr << "BLACK WINS" << std::endl;
				result.forecast = result::loss;
			}
			else {
				std::cerr << std::endl << "WHITE WINS" << std::endl;
				result.forecast = result::win;
			}
			return result;
		}
		else {
			if( !p.white() ) {
				std::cout << std::endl;
			}
			std::cerr << "DRAW" << std::endl;
			result.forecast = result::draw;
			return result;
		}
	}

	// Go through them sorted by previous evaluation. This way, if on time pressure,
	// we can abort processing at high depths early if needed.
	sorted_moves sorted;

	uint64_t hash = get_zobrist_hash( p );

	{
		move tt_move;
		short tmp = 0;
		transposition_table.lookup( hash, 0, 0, 0, 0, tmp, tt_move, tmp );
		for( move_info* it = moves; it != pm; ++it ) {
			move_data md;
			md.m = *it;

			md.pv[0] = md.m.m;

			if( it->m == tt_move ) {
				sorted.insert( sorted.begin(), md );
			}
			else {
				sorted.push_back( md );
			}
		}
	}

	timestamp start;

	short ev = evaluate_full( p );
	new_best_cb.on_new_best_move( p, 1, 0, ev, 0, duration(), sorted.front().pv );

	result.best_move = moves->m;
	if( moves + 1 == pm && !ponder ) {
		result.forecast = ev;
		return result;
	}

	short alpha_at_prev_depth = result::loss;
	int highest_depth = 0;
	int min_depth = 2 + (max_depth % 2);
	if( max_depth < min_depth ) {
		min_depth = max_depth;
	}

	for( int depth = min_depth; depth <= max_depth && !do_abort; ++depth ) {
		short alpha = result::loss;
		short beta = result::win;

		std::size_t evaluated = 0;

		bool got_first_result = false;

		for( sorted_moves::iterator it = sorted.begin(); it != sorted.end(); ++it ) {
			it->s = move_data::waiting;
		}

		push_pv_to_tt( sorted.begin()->pv, p, clock );

		bool done = false;
		while( !done ) {
			scoped_lock l( impl_->mtx_ );

			for( sorted_moves::iterator it = sorted.begin(); it != sorted.end() && !do_abort; ++it ) {

				if( it->s != move_data::waiting ) {
					continue;
				}

				if( !got_first_result && it != sorted.begin() ) {
					// Need to wait for first result
					break;
				}

				std::size_t t;
				for( t = 0; t < impl_->threads_.size(); ++t ) {
					if( !impl_->threads_[t]->is_idle() ) {
						continue;
					}

					it->s = move_data::busy;
					impl_->threads_[t]->process( l, p, hash, it->m, depth, conf.quiescence_depth, alpha_at_prev_depth, alpha, beta, clock, seen );

					// First one is run on its own to get a somewhat sane lower bound for others to work with.
					if( !got_first_result && it == sorted.begin() ) {
						goto break2;
					}
					break;
				}

				if( t == impl_->threads_.size() ) {
					break;
				}
			}
break2:
			if( do_abort ) {
				goto label_abort;
			}

			if( !ponder ) {
				timestamp now;
				if( time_limit > now - start ) {
					impl_->cond_.wait( l, start + time_limit - now );
				}

				now = timestamp();
				if( !do_abort && (now - start) > time_limit ) {
					std::cerr << "Triggering search abort due to time limit at depth " << depth << std::endl;
					do_abort = true;
				}
			}
			else {
				impl_->cond_.wait( l );
			}

			bool all_idle = true;
			for( std::size_t t = 0; t < impl_->threads_.size(); ++t ) {

				if( impl_->threads_[t]->got_results() ) {
					got_first_result = true;
					short value = impl_->threads_[t]->result();
					if( !do_abort ) {
						++evaluated;

						move_info mi = impl_->threads_[t]->get_move();

						sorted_moves::iterator it;
						for( it = sorted.begin(); it != sorted.end(); ++it ) {
							if( it->m.m == mi.m ) {
								it->s = move_data::done;
								break;
							}
						}

						if( value > alpha ) {
							get_pv_from_tt( it->pv, p, depth );

							alpha = value;
							if( mi.m != result.best_move ) {
								if( !ponder && move_time_limit.seconds() >= 1 && depth > 4 && (timestamp() - start) >= (move_time_limit / 10) ) {
									duration extra = move_time_limit / 3;
									if( time_limit + extra > deadline ) {
										extra = deadline - time_limit;
									}
									if( extra.milliseconds() > 0 ) {
										result.used_extra_time += extra;
										time_limit += extra;
										std::cerr << "PV changed, adding " << extra.milliseconds() << " ms extra search time." << std::endl;
									}
								}
								result.best_move = mi.m;
								result.ponder_move = it->pv[1];
							}
							
							// Bubble new best to front
							it->m.sort = value;
							for( ; it != sorted.begin(); --it ) {
								std::swap( *(it - 1), *it );
							}

							highest_depth = depth;

							new_best_cb.on_new_best_move( p, depth, stats.highest_depth(), value, stats.nodes() + stats.quiescence_nodes, timestamp() - start, it->pv );
						}
					}
				}

				if( !impl_->threads_[t]->is_idle() ) {
					all_idle = false;
				}
			}

			if( all_idle ) {
				if( evaluated == sorted.size() ) {
					done = true;
				}
				else if( do_abort ) {
					done = true;
				}
			}
		}
		if( do_abort ) {
			std::cerr << "Aborted with " << evaluated << " of " << sorted.size() << " moves evaluated at current depth" << std::endl;
		}
		else {
			if( alpha < result::loss_threshold || alpha > result::win_threshold ) {
				if( depth < max_depth ) {
					if( alpha > last_mate && !ponder ) {
						std::cerr << "Early break due to mate at " << depth << std::endl;
						do_abort = true;
					}
				}
			}
			else {
				duration elapsed = timestamp() - start;
				if( !ponder && elapsed > (time_limit * 3) / 5 && depth < max_depth ) {
					std::cerr << "Not increasing depth due to time limit. Elapsed: " << elapsed.milliseconds() << " ms" << std::endl;
					do_abort = true;
				}
			}
		}


		if( !sorted.empty() ) {
			alpha_at_prev_depth = alpha;
			result.forecast = alpha;
		}
		else {
			result.forecast = alpha_at_prev_depth;
		}
	}

label_abort:
	scoped_lock l( impl_->mtx_ );

	for( std::vector<processing_thread*>::iterator it = impl_->threads_.begin(); it != impl_->threads_.end(); ++it ) {
		(*it)->wait_idle( l );
	}

	{
		timestamp stop;

		move const* pv = sorted.begin()->pv;
		new_best_cb.on_new_best_move( p, highest_depth, stats.highest_depth(), sorted.begin()->m.sort, stats.nodes() + stats.quiescence_nodes, stop - start, pv );

		stats.print( stop - start );
		stats.accumulate( stop - start );
		stats.reset( false );

		push_pv_to_tt( pv, p, clock );
	}

	return result;
}
