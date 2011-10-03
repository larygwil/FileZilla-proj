#include "assert.hpp"
#include "calc.hpp"
#include "config.hpp"
#include "hash.hpp"
#include "moves.hpp"
#include "eval.hpp"
#include "util.hpp"
#include "platform.hpp"
#include "statistics.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <math.h>
#include <sstream>
#include <string>
#include <map>
#include <vector>

volatile bool do_abort = false;

short const ASPIRATION = 40;

new_best_move_callback default_new_best_move_callback;

namespace {
struct MoveSort {
	bool operator()( move_info const& lhs, move_info const& rhs ) const {
		return lhs.evaluation > rhs.evaluation;
	}
} moveSort;

struct MoveSort2 {
	bool operator()( move_info const& lhs, move_info const& rhs ) const {
		return lhs.sort > rhs.sort;
	}
} moveSort2;
}


void sort_moves( move_info* begin, move_info* end, position const& p, color::type c, short current_evaluation, killer_moves const& killers )
{
	for( move_info* it = begin; it != end; ++it ) {
		it->evaluation = evaluate_move( p, c, current_evaluation, it->m, it->pawns );
		it->sort = it->evaluation;
		if( it->m.captured_piece != pieces::none ) {
			it->sort += get_material_value( it->m.captured_piece ) * 1000000 - get_material_value( it->m.piece );
		}
		else if( killers.is_killer( it->m ) ) {
			it->sort += 500000;
		}
	}
	std::sort( begin, end, moveSort2 );
}


killer_moves const empty_killers;
short quiescence_search( int ply, context& ctx, position const& p, unsigned long long hash, int current_evaluation, check_map const& check, color::type c, short alpha, short beta )
{
#if 0
	if( get_zobrist_hash(p) != hash ) {
		std::cerr << "FAIL HASH!" << std::endl;
	}
	if( evaluate_fast(p, c) != current_evaluation ) {
		std::cerr << "FAIL EVAL!" << std::endl;
	}
	position p2 = p;
	p2.init_pawn_structure();
	if( p2.pawns.hash != p.pawns.hash ) {
		std::cerr << "PAWN HASH FAIL" << std::endl;
	}
#endif
	if( do_abort ) {
		return result::loss;
	}

#ifdef USE_STATISTICS
	++stats.quiescence_nodes;
#endif

	int const limit = MAX_DEPTH + MAX_QDEPTH;

	if( ply >= limit ) {
		return beta;
	}

	// Do lazy evaluation.
	// Assumes that full eval does not differ from fast one by more than 300 centipawns.
	// If this assumption holds, outcome is equivalent to non-lazy eval.
	// This saves between 15-20% of all calls to full_eval.

	short full_eval = result::win; // Any impossible value would do
	if( current_evaluation + 300 > alpha && !check.check ) {
		full_eval = evaluate_full( p, c, current_evaluation );
		if( full_eval > alpha ) {
			if( full_eval >= beta ) {
				return full_eval;
			}
			alpha = full_eval;
		}
	}

	move_info* moves = ctx.move_ptr;

	if( check.check ) {
		calculate_moves( p, c, ctx.move_ptr, check );
		sort_moves( moves, ctx.move_ptr, p, c, current_evaluation, empty_killers );
	}
	else {
		calculate_moves_captures( p, c, ctx.move_ptr, check );
		std::sort( moves, ctx.move_ptr, moveSort );
	}
	bool got_moves = moves != ctx.move_ptr;

	for( move_info* it = moves; it != ctx.move_ptr; ++it ) {
		if( !check.check ) {
			it->evaluation = evaluate_move( p, c, current_evaluation, it->m, it->pawns );
		}

		short value;
		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->m );

		if( ctx.seen.is_two_fold( new_hash, ply ) ) {
			value = result::draw;
		}
		else {
			position new_pos = p;
			apply_move( new_pos, *it, c );
			check_map new_check;
			calc_check_map( new_pos, static_cast<color::type>(1-c), new_check );


			if( ctx.seen.is_two_fold( new_hash, ply ) ) {
				value = result::draw;
			}
			else {
				ctx.seen.pos[ctx.seen.root_position + ply] = new_hash;

				value = -quiescence_search( ply + 1, ctx, new_pos, new_hash, -it->evaluation, new_check, static_cast<color::type>(1-c), -beta, -alpha );
			}
		}
		if( value > alpha ) {
			alpha = value;

			if( alpha >= beta ) {
				break;
			}
		}
	}

	ctx.move_ptr = moves;

	if( !got_moves ) {
		if( check.check ) {
			return result::loss + ply;
		}
		else {
			calculate_moves( p, c, ctx.move_ptr, check );
			if( ctx.move_ptr != moves ) {
				ctx.move_ptr = moves;
				if( full_eval == result::win ) {
					full_eval = evaluate_full( p, c, current_evaluation );
				}
				return full_eval;
			}
			else {
				return result::draw;
			}
		}
	}

	return alpha;
}


short quiescence_search( int ply, context& ctx, position const& p, unsigned long long hash, int current_evaluation, color::type c, short alpha, short beta )
{
	check_map check;
	calc_check_map( p, c, check );

	return quiescence_search( ply, ctx, p, hash, current_evaluation, check, c, alpha, beta );
}

short step( int depth, int ply, context& ctx, position const& p, unsigned long long hash, int current_evaluation, color::type c, short alpha, short beta, pv_entry* pv, bool last_was_null )
{
	if( depth <= 0 ) {
		return quiescence_search( ply, ctx, p, hash, current_evaluation, c, alpha, beta );
	}

	if( do_abort ) {
		return result::loss;
	}

#ifdef USE_STATISTICS
	++stats.full_width_nodes;
#endif

	move tt_move;

	{
		short eval;
		score_type::type t = transposition_table.lookup( hash, c, depth, ply, alpha, beta, eval, tt_move );
		if( t != score_type::none ) {
			if( t == score_type::exact ) {
				ctx.pv_pool.set_pv_move( pv, tt_move );
			}
			return eval;
		}
	}


	check_map check;
	calc_check_map( p, c, check );

#if NULL_MOVE_REDUCTION > 0
	if( !last_was_null && !check.check && depth > 1 && p.material[0] > 1500 && p.material[1] > 1500 ) {
		int old_null = ctx.seen.null_move_position;
		ctx.seen.null_move_position = ctx.seen.root_position + ply - 1;
		pv_entry* cpv = ctx.pv_pool.get();
		short value = -step( depth - NULL_MOVE_REDUCTION - 1, ply + 1, ctx, p, hash, -current_evaluation, static_cast<color::type>(1-c), -beta, -beta + 1, cpv, true );
		ctx.pv_pool.release( cpv );

		ctx.seen.null_move_position = old_null;

		if( value >= beta ) {
			if( !do_abort ) {
				transposition_table.store( hash, c, depth, ply, value, alpha, beta, tt_move, ctx.clock );
			}
			return value;
		}
	}
#endif

	if( !(tt_move.flags & move_flags::valid) && depth > 2 ) {

		step( depth - 2, ply, ctx, p, hash, current_evaluation, c, alpha, beta, pv, false );

		short eval;
		transposition_table.lookup( hash, c, depth, ply, alpha, beta, eval, tt_move );
	}

	short old_alpha = alpha;

	pv_entry* best_pv = 0;

	if( tt_move.flags & move_flags::valid ) {
		position new_pos = p;
		if( apply_move( new_pos, tt_move, c ) ) {
			unsigned long long new_hash = update_zobrist_hash( p, c, hash, tt_move );
			position::pawn_structure pawns;
			short new_eval = evaluate_move( p, c, current_evaluation, tt_move, pawns );

			pv_entry* cpv = ctx.pv_pool.get();

			short value;
			if( ctx.seen.is_two_fold( new_hash, ply ) ) {
				value = result::draw;
			}
			else {
				ctx.seen.pos[ctx.seen.root_position + ply] = new_hash;

				value = -step( depth - 1, ply + 1, ctx, new_pos, new_hash, -new_eval, static_cast<color::type>(1-c), -beta, -alpha, cpv, false );
			}
			if( value > alpha ) {

				alpha = value;

				if( alpha >= beta ) {
					ctx.pv_pool.release(cpv);

					if( !do_abort ) {
						transposition_table.store( hash, c, depth, ply, alpha, old_alpha, beta, tt_move, ctx.clock );
					}
					return alpha;
				}
				else {
					best_pv = cpv;
				}
			}
			else {
				ctx.pv_pool.release(cpv);
			}
		}
		else {
			// Unfortunate type-1 collision
			std::cerr << "Possible type-1 collision in full-width search" << std::endl;
			tt_move.flags = 0;
		}
	}

	move_info* moves = ctx.move_ptr;
	calculate_moves( p, c, ctx.move_ptr, check );

	if( ctx.move_ptr == moves ) {
		if( best_pv ) {
			ctx.pv_pool.release(best_pv);
		}
		ASSERT( !pv->get_best_move().other )
		if( check.check ) {
			return result::loss + ply;
		}
		else {
			return result::draw;
		}
	}

	sort_moves( moves, ctx.move_ptr, p, c, current_evaluation, ctx.killers[c][ply] );

	for( move_info* it = moves; it != ctx.move_ptr; ++it ) {
		if( tt_move.flags & move_flags::valid && tt_move == it->m ) {
			continue;
		}

		position new_pos = p;
		apply_move( new_pos, *it, c );
		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->m );
		short value;

		pv_entry* cpv = ctx.pv_pool.get();
		if( ctx.seen.is_two_fold( new_hash, ply ) ) {
			value = result::draw;
		}
		else {
			ctx.seen.pos[ctx.seen.root_position + ply] = new_hash;

			if( alpha != old_alpha ) {
				value = -step( depth - 1, ply + 1, ctx, new_pos, new_hash, -it->evaluation, static_cast<color::type>(1-c), -alpha-1, -alpha, cpv, false );

				if( value > alpha ) {
					value = -step( depth - 1, ply + 1, ctx, new_pos, new_hash, -it->evaluation, static_cast<color::type>(1-c), -beta, -alpha, cpv, false );
				}
			}
			else {
				value = -step( depth - 1, ply + 1, ctx, new_pos, new_hash, -it->evaluation, static_cast<color::type>(1-c), -beta, -alpha, cpv, false );
			}
		}
		if( value > alpha ) {
			alpha = value;
			tt_move = it->m;

			if( alpha >= beta ) {
				ctx.pv_pool.release(cpv);

				if( !it->m.captured_piece ) {
					ctx.killers[c][ply].add_killer( it->m );
				}
				break;
			}
			else {
				if( best_pv ) {
					ctx.pv_pool.release(best_pv);
				}
				best_pv = cpv;
			}
		}
		else {
			ctx.pv_pool.release(cpv);
		}
	}

	ctx.move_ptr = moves;

	if( alpha < beta && alpha > old_alpha ) {
		ctx.pv_pool.append( pv, tt_move, best_pv );
	}
	else if( best_pv ) {
		ctx.pv_pool.release( best_pv );
	}

	if( !do_abort ) {
		transposition_table.store( hash, c, depth, ply, alpha, old_alpha, beta, tt_move, ctx.clock );
	}

	return alpha;
}

namespace {
class processing_thread : public thread
{
public:
	enum state_type {
		idle,
		busy,
		pending_results,
		quitting
	};

	processing_thread( mutex& mtx, condition& cond )
		: mutex_(mtx), cond_(cond), clock_(), state_(idle)
	{
		spawn();
	}

	void quit( scoped_lock& l )
	{
		state_ = quitting;
		waiting_on_work_.signal( l );
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
		state_ = idle;
		return result_;
	}

	void process( scoped_lock& l, position const& p, color::type c, move_info const& m, short max_depth, short quiescence_depth, short alpha_at_prev_depth, short alpha, short beta, int clock, pv_entry* pv, seen_positions const& seen )
	{
		p_ = p;
		c_ = c;
		m_ = m;
		max_depth_ = max_depth;
		quiescence_depth_ = quiescence_depth;
		alpha_at_prev_depth_ = alpha_at_prev_depth;
		alpha_ = alpha;
		beta_ = beta;
		clock_ = clock;
		pv_ = pv;

		seen_ = seen;

		state_ = busy;

		waiting_on_work_.signal( l );
	}

	move_info get_move() const { return m_; }

	// Call locked and when finished
	pv_entry* get_pv() const {
		return pv_;
	}

	virtual void onRun();
private:
	short processWork();

	mutex& mutex_;
	condition& cond_;

	condition waiting_on_work_;

	position p_;
	color::type c_;
	move_info m_;
	short max_depth_;
	short quiescence_depth_;
	short alpha_at_prev_depth_;
	short alpha_;
	short beta_;

	short result_;
	int clock_;

	pv_entry* pv_;

	seen_positions seen_;

	context ctx_;

	state_type state_;
};
}


short processing_thread::processWork()
{
	position new_pos = p_;
	apply_move( new_pos, m_, c_ );
	unsigned long long hash = get_zobrist_hash( new_pos );

	ctx_.clock = clock_ % 256;
	ctx_.seen = seen_;
	ctx_.move_ptr = ctx_.moves;

	if( ctx_.seen.is_two_fold( hash, 1 ) ) {
		ctx_.pv_pool.clear_pv_move( pv_->next() );

		return result::draw;
	}

	ctx_.seen.pos[++ctx_.seen.root_position] = hash;

	// Search using aspiration window:
	short value;
	if( alpha_ == result::loss && alpha_at_prev_depth_ != result::loss ) {
		// Windows headers unfortunately create some defines called max and min :(
		short alpha = (std::max)( alpha_, static_cast<short>(alpha_at_prev_depth_ - ASPIRATION) );
		short beta = (std::min)( beta_, static_cast<short>(alpha_at_prev_depth_ + ASPIRATION) );

		if( alpha < beta ) {
			value = -step( max_depth_, 1, ctx_, new_pos, hash, -m_.evaluation, static_cast<color::type>(1-c_), -beta, -alpha, pv_->next(), false );
			if( value > alpha && value < beta ) {
				// Aspiration search found something sensible
				return value;
			}
		}
	}

	if( alpha_ != result::loss ) {
		value = -step( max_depth_, 1, ctx_, new_pos, hash, -m_.evaluation, static_cast<color::type>(1-c_), -alpha_-1, -alpha_, pv_->next(), false );
		if( value > alpha_ ) {
			value = -step( max_depth_, 1, ctx_, new_pos, hash, -m_.evaluation, static_cast<color::type>(1-c_), -beta_, -alpha_, pv_->next(), false );
		}
	}
	else {
		value = -step( max_depth_, 1, ctx_, new_pos, hash, -m_.evaluation, static_cast<color::type>(1-c_), -beta_, -alpha_, pv_->next(), false );
	}

	return value;
}


void processing_thread::onRun()
{
	scoped_lock l(mutex_);

	while( true ) {

		while( state_ != busy && state_ != quitting ) {
			waiting_on_work_.wait( l );
		}

		if( state_ == quitting ) {
			return;
		}

		l.unlock();

		short result = processWork();

		l.lock();

		result_ = result;
		state_ = pending_results;
		cond_.signal( l );
	}
}

struct move_data {
	short forecast;
	move_info m;
	pv_entry* pv;
};

typedef std::vector<move_data> sorted_moves;
void insert_sorted( sorted_moves& moves, int forecast, move_info const& m, pv_entry* pv )
{
	sorted_moves::iterator it = moves.begin();
	while( it != moves.end() && it->forecast >= forecast ) {
		++it;
	}
	move_data d;
	d.forecast = forecast;
	d.m = m;
	d.pv = pv;
	moves.insert( it, d );
}

bool calc( position& p, color::type c, move& m, int& res, unsigned long long move_time_limit, unsigned long long /*time_remaining*/, int clock, seen_positions& seen
		  , short last_mate
		  , new_best_move_callback& new_best_cb )
{
	if( move_time_limit ) {
		std::cerr << "Current move time limit is " << 1000 * move_time_limit / timer_precision() << " ms" << std::endl;
	}
	else {
		std::cerr << "Pondering..." << std::endl;
	}

	do_abort = false;
	check_map check;
	calc_check_map( p, c, check );

	move_info moves[200];
	move_info* pm = moves;
	int current_evaluation = evaluate_fast( p, c );
	calculate_moves( p, c, pm, check );
	sort_moves( moves, pm, p, c, current_evaluation, empty_killers );

	if( moves == pm ) {
		if( check.check ) {
			if( c == color::white ) {
				std::cerr << "BLACK WINS" << std::endl;
				res = result::loss;
			}
			else {
				std::cerr << std::endl << "WHITE WINS" << std::endl;
				res = result::win;
			}
			return false;
		}
		else {
			if( c == color::black ) {
				std::cout << std::endl;
			}
			std::cerr << "DRAW" << std::endl;
			res = result::draw;
			return false;
		}
	}

	// Go through them sorted by previous evaluation. This way, if on time pressure,
	// we can abort processing at high depths early if needed.
	sorted_moves old_sorted;

	{
		pv_entry_pool pool;
		for( move_info* it  = moves; it != pm; ++it ) {
			pv_entry* pv = pool.get();
			pool.append( pv, it->m, pool.get() );
			// Initial order is random to get some variation into play
			insert_sorted( old_sorted, static_cast<int>(get_random_unsigned_long_long()), *it, pv );
		}
	}

	unsigned long long start = get_time();

	mutex mtx;

	condition cond;

	std::vector<processing_thread*> threads;
	for( int t = 0; t < conf.thread_count; ++t ) {
		threads.push_back( new processing_thread( mtx, cond ) );
	}

	short alpha_at_prev_depth = result::loss;
	int highest_depth = 0;
	for( int max_depth = 2 + (conf.depth % 2); max_depth <= conf.depth; ++max_depth )
	{
		short alpha = result::loss;
		short beta = result::win;

		sorted_moves sorted;

		sorted_moves::const_iterator it = old_sorted.begin();
		int evaluated = 0;

		bool got_first_result = false;

		bool done = false;
		while( !done ) {

			scoped_lock l( mtx );

			while( it != old_sorted.end() && !do_abort ) {

				if( !got_first_result && it != old_sorted.begin() ) {
					// Need to wait for first result
					break;
				}

				std::size_t t;
				for( t = 0; t < threads.size(); ++t ) {
					if( !threads[t]->is_idle() ) {
						continue;
					}

					threads[t]->process( l, p, c, it->m, max_depth, conf.quiescence_depth, alpha_at_prev_depth, alpha, beta, clock, it->pv, seen );

					// First one is run on its own to get a somewhat sane lower bound for others to work with.
					if( it++ == old_sorted.begin() ) {
						goto break2;
					}
					break;
				}

				if( t == threads.size() ) {
					break;
				}
			}
break2:

			if( move_time_limit ) {
				unsigned long long now = get_time();
				if( move_time_limit > now - start ) {
					cond.wait( l, move_time_limit - now + start );
				}

				now = get_time();
				if( !do_abort && move_time_limit > 0 && (now - start) > move_time_limit  ) {
					std::cerr << "Triggering search abort due to time limit at depth " << max_depth << std::endl;
					do_abort = true;
				}
			}
			else {
				cond.wait( l );
			}

			bool all_idle = true;
			for( std::size_t t = 0; t < threads.size(); ++t ) {

				if( threads[t]->got_results() ) {
					got_first_result = true;
					int value = threads[t]->result();
					if( !do_abort ) {
						++evaluated;

						pv_entry* pv = threads[t]->get_pv();
						extend_pv_from_tt( pv, p, c, max_depth, conf.quiescence_depth );

						insert_sorted( sorted, value, threads[t]->get_move(), pv );
						if( threads[t]->get_move().m != pv->get_best_move() ) {
							std::cerr << "FAIL: Wrong PV move" << std::endl;
						}

						if( value > alpha ) {
							alpha = value;

							highest_depth = max_depth;
							new_best_cb.on_new_best_move( p, c, max_depth, value, stats.full_width_nodes + stats.quiescence_nodes, pv );
						}
					}
				}

				if( !threads[t]->is_idle() ) {
					all_idle = false;
				}
			}

			if( all_idle ) {
				if( it == old_sorted.end() ) {
					done = true;
				}
				else if( do_abort ) {
					done = true;
				}
			}
		}
		if( do_abort ) {
			std::cerr << "Aborted with " << evaluated << " of " << old_sorted.size() << " moves evaluated at current depth" << std::endl;
		}
		else {
			if( alpha < result::loss_threshold || alpha > result::win_threshold ) {
				if( max_depth < conf.depth ) {
					if( alpha > last_mate) {
						std::cerr << "Early break due to mate at " << max_depth << std::endl;
						do_abort = true;
					}
				}
			}
			else {
				unsigned long long now = get_time();
				unsigned long long elapsed = now - start;
				if( move_time_limit && elapsed * 2 > move_time_limit ) {
					std::cerr << "Not increasing depth due to time limit. Elapsed: " << elapsed * 1000 / timer_precision() << " ms" << std::endl;
					do_abort = true;
				}
			}
		}


		if( !sorted.empty() ) {
			if( !do_abort && old_sorted.size() != sorted.size() ) {
				std::cerr << "FAIL: A move gone missing! " << old_sorted.size() << " " << sorted.size() << std::endl;
			}

			if( old_sorted.size() ) {
				bool found_old_best = false;
				for( sorted_moves::const_iterator it = sorted.begin(); it != sorted.end(); ++it ) {
					if( it->m.m == old_sorted.begin()->m.m ) {
						found_old_best = true;
					}
				}
				if( !found_old_best ) {
					std::cerr << "FAIL: Old best move not evaluated?" << std::endl;
				}
			}
			sorted.swap( old_sorted );
			alpha_at_prev_depth = alpha;
			res = alpha;
		}
		else {
			res = alpha_at_prev_depth;
		}
		if( do_abort ) {
			break;
		}
	}

/*	std::cerr << "Candidates: " << std::endl;
	for( sorted_moves::const_iterator it = old_sorted.begin(); it != old_sorted.end(); ++it ) {
		if( it->pv->get_best_move() != it->m.m ) {
			std::cerr << "FAIL: Wrong PV move" << std::endl;
		}
		std::stringstream ss;
		ss << std::setw( 7 ) << it->forecast;
		std::cerr << ss.str() << " " << pv_to_string( it->pv, p, c ) << std::endl;

		pv_entry_pool p;
		p.release( it->pv );
	}*/

	m = old_sorted.begin()->m.m;

	pv_entry const* pv = old_sorted.begin()->pv;
	new_best_cb.on_new_best_move( p, c, highest_depth, old_sorted.begin()->forecast, stats.full_width_nodes + stats.quiescence_nodes, pv );

	unsigned long long stop = get_time();
	print_stats( start, stop );
	reset_stats();

	scoped_lock l( mtx );

	for( std::vector<processing_thread*>::iterator it = threads.begin(); it != threads.end(); ++it ) {
		(*it)->quit( l );
	}

	l.unlock();
	for( std::vector<processing_thread*>::iterator it = threads.begin(); it != threads.end(); ++it ) {
		delete *it;
	}

	return true;
}


seen_positions::seen_positions()
	: root_position()
	, null_move_position()
{
	memset( pos, 0, (100 + MAX_DEPTH + MAX_QDEPTH + 10)*8 );
}

bool seen_positions::is_three_fold( unsigned long long hash, int ply ) const
{
	int count = 0;
	for( int i = root_position + ply - 4; i >= null_move_position; i -= 2) {
		if( pos[i] == hash ) {
			++count;
		}
	}
	return count >= 2;
}

bool seen_positions::is_two_fold( unsigned long long hash, int ply ) const
{
	for( int i = root_position + ply - 4; i >= null_move_position; i -= 2 ) {
		if( pos[i] == hash ) {
			return true;
		}
	}
	return false;
}

void new_best_move_callback::on_new_best_move( position const& p, color::type c, int depth, int evaluation, unsigned long long nodes, pv_entry const* pv )
{
	std::stringstream ss;
	ss << "Best so far: " << std::setw(2) << depth << " " << std::setw(7) << evaluation << " " << std::setw(10) << nodes << " " << std::setw(0) << pv_to_string( pv, p, c ) << std::endl;
	std::cerr << ss.str();
}
