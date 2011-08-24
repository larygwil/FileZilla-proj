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
#include <sstream>
#include <string>
#include <map>
#include <vector>

short const ASPIRATION = 40;

namespace {
struct MoveSort {
	bool operator()( move_info const& lhs, move_info const& rhs ) const {
		return lhs.evaluation > rhs.evaluation;
	}
} moveSort;

struct MoveSortForecast {
	bool operator()( move_info const& lhs, move_info const& rhs ) const {
		if( lhs.forecast > rhs.forecast ) {
			return true;
		}
		if( lhs.forecast < rhs.forecast ) {
			return false;
		}

		return lhs.evaluation > rhs.evaluation;
	}
} moveSortForecast;

}


short quiescence_search( int depth, context& ctx, position const& p, unsigned long long hash, int current_evaluation, check_map const& check, color::type c, short alpha, short beta )
{
	int const limit = ctx.max_depth + ctx.quiescence_depth;

	short eval;
	move best_move;
	if( transposition_table.lookup( hash, limit - depth, alpha, beta, eval, best_move, ctx.clock ) ) {
		return eval;
	}

	if( depth >= limit && !check.check )
	{
#ifdef USE_STATISTICS
		++stats.evaluated_leaves;
#endif
		return current_evaluation;
	}

	short old_alpha = alpha;

	bool evaluated_move = false;

	if( best_move.other ) {
		// Not a terminal node, do this check early:
		if( depth >= limit ) {
	#ifdef USE_STATISTICS
			++stats.evaluated_leaves;
	#endif
			return current_evaluation;
		}

		position new_pos = p;
		bool captured;
		if( apply_move( new_pos, best_move, c, captured ) ) {
			check_map new_check;
			calc_check_map( new_pos, static_cast<color::type>(1-c), new_check );
			position::pawn_structure pawns;
			short new_eval = evaluate_move( p, c, current_evaluation, best_move, pawns );

			short value;
			if( check.check || captured || new_check.check ) {

				evaluated_move = true;

				unsigned long long new_hash = update_zobrist_hash( p, c, hash, best_move );
				value = -quiescence_search( depth + 1, ctx, new_pos, new_hash, -new_eval, new_check, static_cast<color::type>(1-c), -beta, -alpha );

				if( value > alpha ) {

					alpha = value;

					if( alpha >= beta ) {
		#ifdef USE_STATISTICS
						++stats.evaluated_intermediate;
		#endif

						transposition_table.store( hash, limit - depth, alpha, old_alpha, beta, best_move, ctx.clock );
						return alpha;
					}
				}
			}
		}
		else {
			best_move.other = 0;
		}
	}

	move_info moves[200];
	move_info* pm = moves;

	if( check.check ) {
		calculate_moves( p, c, current_evaluation, pm, check );
		std::sort( moves, pm, moveSort );
	}
	else {
		inverse_check_map inverse_check;
		calc_inverse_check_map( p, c, inverse_check );

		calculate_moves_captures_and_checks( p, c, current_evaluation, pm, check, inverse_check );
		if( pm == moves ) {
			calculate_moves( p, c, current_evaluation, pm, check );
			if( pm != moves ) {
				return current_evaluation;
			}
		}
	}

	if( pm == moves ) {
		ASSERT( !best_move.other || d.remaining_depth == 31 );
		if( check.check ) {
#ifdef USE_STATISTICS
			++stats.evaluated_leaves;
#endif
			return result::loss + depth;
		}
		else {
#ifdef USE_STATISTICS
			++stats.evaluated_leaves;
#endif
			return result::draw;
		}
	}

	if( depth >= limit ) {
#ifdef USE_STATISTICS
		++stats.evaluated_leaves;
#endif
		return current_evaluation;
	}


#ifdef USE_STATISTICS
	++stats.evaluated_intermediate;
#endif

	if( !best_move.other ) {
		best_move = moves->m;
	}

	++depth;

	for( move_info const* it = moves; it != pm; ++it ) {
		if( best_move.other && best_move == it->m ) {
			continue;
		}
		position new_pos = p;
		bool captured;
		apply_move( new_pos, *it, c, captured );
		check_map new_check;
		calc_check_map( new_pos, static_cast<color::type>(1-c), new_check );
		short value;

		if( check.check || captured || new_check.check ) {

			evaluated_move = true;

			unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->m );
			if( best_move.other || it != moves ) {
				value = -quiescence_search( depth, ctx, new_pos, new_hash, -it->evaluation, new_check, static_cast<color::type>(1-c), -alpha-1, -alpha );
				if( value > alpha && value < beta ) {
					value = -quiescence_search( depth, ctx, new_pos, new_hash, -it->evaluation, new_check, static_cast<color::type>(1-c), -beta, -alpha );
				}
			}
			else {
				value = -quiescence_search( depth, ctx, new_pos, new_hash, -it->evaluation, new_check, static_cast<color::type>(1-c), -beta, -alpha );
			}
			if( value > alpha ) {
				alpha = value;

				best_move = it->m;

				if( alpha >= beta )
					break;
			}
		}
	}

	if( !evaluated_move ) {
		alpha = current_evaluation;
	}
	else if( !check.check && current_evaluation > alpha ) {
		// Avoid capture line
		alpha = current_evaluation;
	}

	transposition_table.store( hash, limit - depth + 1, alpha, old_alpha, beta, best_move, ctx.clock );

	return alpha;
}


short quiescence_search( int depth, context& ctx, position const& p, unsigned long long hash, int current_evaluation, color::type c, short alpha, short beta )
{
	check_map check;
	calc_check_map( p, c, check );

	return quiescence_search( depth, ctx, p, hash, current_evaluation, check, c, alpha, beta );
}


short step( int depth, context& ctx, position const& p, unsigned long long hash, int current_evaluation, color::type c, short alpha, short beta, pv_entry* pv )
{
	{
		short eval;
		move best_move;
		if( transposition_table.lookup( hash, ctx.max_depth - depth + 128, alpha, beta, eval, best_move, ctx.clock ) ) {
			ctx.pv_pool.set_pv_move( pv, best_move );
			return eval;
		}
		if( best_move.other ) {
			ctx.pv_pool.set_pv_move( pv, best_move );
		}
	}

	if( depth >= ctx.max_depth ) {
		return quiescence_search( depth, ctx, p, hash, current_evaluation, c, alpha, beta );
	}

	if( !pv->get_best_move().other && depth + 2 <= ctx.max_depth ) {
		ctx.max_depth -= 2;
		step( depth, ctx, p, hash, current_evaluation, c, alpha, beta, pv );
		ctx.max_depth += 2;
	}

	short old_alpha = alpha;

	if( pv->get_best_move().other ) {
		position new_pos = p;
		bool captured;
		if( apply_move( new_pos, pv->get_best_move(), c, captured ) ) {
			unsigned long long new_hash = update_zobrist_hash( p, c, hash, pv->get_best_move() );
			position::pawn_structure pawns;
			short new_eval = evaluate_move( p, c, current_evaluation, pv->get_best_move(), pawns );

			pv_entry* cpv = pv->next();
			if( !cpv ) {
				cpv = ctx.pv_pool.get();
			}

			short value;
			if( ctx.seen.is_two_fold( new_hash, depth + 1 ) ) {
				value = result::draw;
				ctx.pv_pool.clear_pv_move( cpv );
			}
			else {
				ctx.seen.pos[ctx.seen.root_position + depth + 1] = new_hash;

				value = -step( depth + 1, ctx, new_pos, new_hash, -new_eval, static_cast<color::type>(1-c), -beta, -alpha, cpv );
			}
			if( value > alpha ) {

				if( !pv->next() ) {
					ctx.pv_pool.append( pv, pv->get_best_move(), cpv );
				}
				alpha = value;

				if( alpha >= beta ) {
	#ifdef USE_STATISTICS
					++stats.evaluated_intermediate;
	#endif

					transposition_table.store( hash, ctx.max_depth - depth + 128, alpha, old_alpha, beta, pv->get_best_move(), ctx.clock );
					return alpha;
				}
			}
			else {
				if( !pv->next() ) {
					ctx.pv_pool.release(cpv);
				}
			}
		}
		else {
			// Unfortunate type-1 collision
			std::cerr << "Possible type-1 collision" << std::endl;
			ctx.pv_pool.clear_pv_move( pv );
			//goto retry_after_type1_collision;
		}
	}

	check_map check;
	calc_check_map( p, c, check );

	move_info moves[200];
	move_info* pm = moves;
	calculate_moves( p, c, current_evaluation, pm, check );

	if( pm == moves ) {
		ASSERT( !pv->get_best_move().other )
		if( check.check ) {
			return result::loss + depth;
		}
		else {
			return result::draw;
		}
	}

#ifdef USE_STATISTICS
	++stats.evaluated_intermediate;
#endif

	++depth;

	std::sort( moves, pm, moveSort );

	for( move_info const* it = moves; it != pm; ++it ) {
		if( pv->get_best_move().other && pv->get_best_move() == it->m ) {
			continue;
		}

		position new_pos = p;
		bool captured;
		apply_move( new_pos, it->m, c, captured );
		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->m );
		short value;

		pv_entry* cpv = ctx.pv_pool.get();
		if( ctx.seen.is_two_fold( new_hash, depth ) ) {
			value = result::draw;
		}
		else {
			ctx.seen.pos[ctx.seen.root_position + depth] = new_hash;

			if( pv->get_best_move().other ) {
				value = -step( depth, ctx, new_pos, new_hash, -it->evaluation, static_cast<color::type>(1-c), -alpha-1, -alpha, cpv );

				if( value > alpha && value < beta) {
					value = -step( depth, ctx, new_pos, new_hash, -it->evaluation, static_cast<color::type>(1-c), -beta, -alpha, cpv );
				}
			}
			else {
				value = -step( depth, ctx, new_pos, new_hash, -it->evaluation, static_cast<color::type>(1-c), -beta, -alpha, cpv );
			}
		}
		if( value > alpha ) {
			ctx.pv_pool.append( pv, it->m, cpv );
			alpha = value;

			if( alpha >= beta )
				break;
		}
		else {
			ctx.pv_pool.release(cpv);
		}
	}

	if( !pv->get_best_move().other ) {
		ctx.pv_pool.set_pv_move( pv, moves->m );
	}

	transposition_table.store( hash, ctx.max_depth - depth + 128 + 1, alpha, old_alpha, beta, pv->get_best_move(), ctx.clock );

	return alpha;
}

namespace {
class processing_thread : public thread
{
public:
	processing_thread( mutex& mtx, condition& cond )
		: mutex_(mtx), cond_(cond), finished_(), clock_()
	{
	}

	// Call locked
	bool finished() {
		return finished_;
	}

	// Call locked
	short result() {
		return result_;
	}

	void process( position const& p, color::type c, move_info const& m, short max_depth, short quiescence_depth, short alpha_at_prev_depth, short alpha, short beta, int clock, pv_entry* pv, seen_positions const& seen )
	{
		p_ = p;
		c_ = c;
		m_ = m;
		max_depth_ = max_depth;
		quiescence_depth_ = quiescence_depth;
		alpha_at_prev_depth_ = alpha_at_prev_depth;
		alpha_ = alpha;
		beta_ = beta;
		finished_ = false;
		clock_ = clock;
		pv_ = pv;

		seen_ = seen;

		spawn();
	}

	move_info get_move() const { return m_; }

	// Call locked and when finished
	pv_entry* get_pv() const {
		return pv_;
	}

	virtual void onRun();
private:
	mutex& mutex_;
	condition& cond_;

	position p_;
	color::type c_;
	move_info m_;
	short max_depth_;
	short quiescence_depth_;
	short alpha_at_prev_depth_;
	short alpha_;
	short beta_;

	bool finished_;
	short result_;
	int clock_;

	pv_entry* pv_;

	seen_positions seen_;
};
}


void processing_thread::onRun()
{
	position new_pos = p_;
	bool captured;
	apply_move( new_pos, m_.m, c_, captured );
	unsigned long long hash = get_zobrist_hash( new_pos, static_cast<color::type>(1-c_) );

	context ctx;
	ctx.max_depth = max_depth_;
	ctx.quiescence_depth = quiescence_depth_;
	ctx.clock = clock_ % 256;
	ctx.seen = seen_;

	if( ctx.seen.is_two_fold( hash, 0 ) ) {
		ctx.pv_pool.clear_pv_move( pv_->next() );

		scoped_lock l( mutex_ );
		result_ = result::draw;
		finished_ = true;
		cond_.signal( l );

		return;
	}

	ctx.seen.pos[++ctx.seen.root_position] = hash;

	// Search using aspiration window:
	short value;
	if( alpha_at_prev_depth_ != result::loss ) {
		short alpha = std::max( alpha_, static_cast<short>(alpha_at_prev_depth_ - ASPIRATION) );
		short beta = std::min( beta_, static_cast<short>(alpha_at_prev_depth_ + ASPIRATION) );

		value = -step( 1, ctx, new_pos, hash, -m_.evaluation, static_cast<color::type>(1-c_), -beta, -alpha, pv_->next() );
		if( value > alpha && value < beta ) {
			// Aspiration search found something sensible
			scoped_lock l( mutex_ );
			result_ = value;
			finished_ = true;
			cond_.signal( l );
			return;
		}
	}

	if( alpha_ != result::loss ) {
		value = -step( 1, ctx, new_pos, hash, -m_.evaluation, static_cast<color::type>(1-c_), -alpha_-1, -alpha_, pv_->next() );
		if( value > alpha_ ) {
			value = -step( 1, ctx, new_pos, hash, -m_.evaluation, static_cast<color::type>(1-c_), -beta_, -alpha_, pv_->next() );
		}
	}
	else {
		value = -step( 1, ctx, new_pos, hash, -m_.evaluation, static_cast<color::type>(1-c_), -beta_, -alpha_, pv_->next() );
	}

	scoped_lock l( mutex_ );
	result_ = value;
	finished_ = true;
	cond_.signal( l );
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

bool calc( position& p, color::type c, move& m, int& res, int time_limit, int clock, seen_positions& seen )
{
	check_map check;
	calc_check_map( p, c, check );

	move_info moves[200];
	move_info* pm = moves;
	int current_evaluation = evaluate( p, c );
	calculate_moves( p, c, current_evaluation, pm, check );

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

//	{
//		std::cerr << "Possible moves:";
//		move* mbegin = &moves[0];
//		move* mend = mbegin + count;
//		for( ; mbegin != mend; ++mbegin ) { // Optimize this, compiler!
//			std::cerr << " " << move_to_string( p, c, *mbegin );
//		}
//		std::cerr << std::endl;
//	}

	// Go through them sorted by previous evaluation. This way, if on time pressure,
	// we can abort processing at high depths early if needed.
	sorted_moves old_sorted;

	{
		pv_entry_pool pool;
		for( move_info const* it  = moves; it != pm; ++it ) {
			pv_entry* pv = pool.get();
			pool.append( pv, it->m, pool.get() );
			insert_sorted( old_sorted, it - moves, *it, pv );
		}
	}

	unsigned long long start = get_time();

	mutex mtx;

	condition cond;

	std::vector<processing_thread*> threads;
	for( int t = 0; t < conf.thread_count; ++t ) {
		threads.push_back( new processing_thread( mtx, cond ) );
	}

	unsigned long long previous_loop_duration = 0;

	short alpha_at_prev_depth = result::loss;
	for( int max_depth = 2 + (conf.depth % 2); max_depth <= conf.depth; max_depth += 2 )
	{
		unsigned long long loop_start = start;

		short alpha = result::loss;
		short beta = result::win;

		sorted_moves sorted;

		sorted_moves::const_iterator it = old_sorted.begin();
		int evaluated = 0;

		bool abort = false;
		bool done = false;
		while( !done ) {
			while( it != old_sorted.end() && !abort ) {
				int t;
				for( t = 0; t < conf.thread_count; ++t ) {
					if( threads[t]->spawned() ) {
						continue;
					}

					threads[t]->process( p, c, it->m, max_depth, conf.quiescence_depth, alpha_at_prev_depth, alpha, beta, clock, it->pv, seen );

					// First one is run on its own to get a somewhat sane lower bound for others to work with.
					if( it++ == old_sorted.begin() ) {
						goto break2;
					}
					break;
				}

				if( t == conf.thread_count ) {
					break;
				}
			}
break2:

			scoped_lock l( mtx );
			cond.wait( l );

			unsigned long long now = get_time();
			if( !abort && time_limit > 0 && (now - start) > time_limit ) {
				std::cerr << "Triggering search abort due to time limit at depth " << max_depth << std::endl;
				abort = true;
			}

			bool all_idle = true;
			for( int t = 0; t < conf.thread_count; ++t ) {
				if( !threads[t]->spawned() ) {
					continue;
				}

				if( !threads[t]->finished() ) {
					all_idle = false;
					continue;
				}

				threads[t]->join();
				int value = threads[t]->result();
				++evaluated;
				if( value > alpha ) {
					alpha = value;
				}

				pv_entry* pv = threads[t]->get_pv();
				insert_sorted( sorted, value, threads[t]->get_move(), pv );
			}

			if( all_idle ) {
				if( it == old_sorted.end() ) {
					done = true;
				}
				else if( abort ) {
					done = true;
				}
			}
		}
		if( abort ) {
			std::cerr << "Aborted with " << evaluated << " moves evaluated at current depth" << std::endl;
		}

		res = alpha;
		if( alpha < result::loss_threshold || alpha > result::win_threshold ) {
			if( max_depth < conf.depth ) {
				std::cerr << "Early break at " << max_depth << std::endl;
			}
			abort = true;
		}

		alpha_at_prev_depth = alpha;

		if( !sorted.empty() ) {
			sorted.swap( old_sorted );
		}
		if( abort ) {
			break;
		}

		unsigned long long loop_duration = get_time() - loop_start;
		if( previous_loop_duration ) {
			unsigned long long depth_factor = loop_duration / previous_loop_duration;
			std::cerr << "Search depth increment time factor is " << depth_factor << std::endl;
		}
		previous_loop_duration = loop_duration;

	}

	std::cerr << "Candidates: " << std::endl;
	for( sorted_moves::const_iterator it = old_sorted.begin(); it != old_sorted.end(); ++it ) {
		std::stringstream ss;
		ss << std::setw( 7 ) << it->forecast;
		std::cerr << ss.str() << " " << pv_to_string( it->pv, p, c ) << std::endl;

		pv_entry_pool p;
		p.release( it->pv );
	}

	m = old_sorted.begin()->m.m;

	unsigned long long stop = get_time();
	print_stats( start, stop );
	reset_stats();

	for( std::vector<processing_thread*>::iterator it = threads.begin(); it != threads.end(); ++it ) {
		delete *it;
	}

	return true;
}


bool seen_positions::is_three_fold( unsigned long long hash, int depth ) const
{
	int count = 0;
	for( int i = root_position + depth - 1; i >= 0; --i ) {
		if( pos[i] == hash ) {
			++count;
		}
	}
	return count >= 2;
}

bool seen_positions::is_two_fold( unsigned long long hash, int depth ) const
{
	for( int i = root_position + depth - 1; i >= 0; --i ) {
		if( pos[i] == hash ) {
			return true;
		}
	}
	return false;
}
