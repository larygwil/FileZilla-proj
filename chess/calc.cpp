#include "assert.hpp"
#include "calc.hpp"
#include "hash.hpp"
#include "moves.hpp"
#include "eval.hpp"
#include "util.hpp"
#include "platform.hpp"
#include "statistics.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <map>

int const MAX_DEPTH = 9;

short step( int depth, int const max_depth, position const& p, unsigned long long hash, int current_evaluation, bool captured, color::type c, short alpha, short beta )
{
#if USE_QUIESCENCE
	int limit = max_depth;
	if( depth >= max_depth && captured ) {
		limit = max_depth + 1;
	}
#else
	int const limit = max_depth;
#endif

	bool got_old_best = false;
	step_data d;

	if( hash && lookup( hash, reinterpret_cast<unsigned char * const>(&d) ) ) {
		if( d.remaining_depth == -127 ) {
			if( d.evaluation == result::loss ) {
#if USE_STATISTICS
				++stats.transposition_table_cutoffs;
#endif
				return result::loss + depth;
			}
			else if( d.evaluation == result::win ) {
#if USE_STATISTICS
				++stats.transposition_table_cutoffs;
#endif
				return result::win - depth;
			}
			else {
#if USE_STATISTICS
				++stats.transposition_table_cutoffs;
#endif
				return result::draw;
			}
		}
//		else if( d.evaluation < result::loss_threshold ) {
//#if USE_STATISTICS
//			++stats.transposition_table_cutoffs;
//#endif
//			return result::loss + depth + 1;
//		}
//		else if( d.evaluation > result::win_threshold ) {
//#if USE_STATISTICS
//			++stats.transposition_table_cutoffs;
//#endif
//			return result::win - depth - 1;
//		}
		else if( (limit - depth) <= d.remaining_depth && alpha >= d.alpha && beta <= d.beta ) {
#if USE_STATISTICS
			++stats.transposition_table_cutoffs;
#endif
			return d.evaluation	;
		}
		got_old_best = true;

		// TODO: Does this have to be cached?
	}

	check_map check;
	calc_check_map( p, c, check );

#if USE_QUIESCENCE
	if( depth >= limit && !check.check )
#else
	if( depth >= max_depth && !check.check )
#endif
	{
#ifdef USE_STATISTICS
		++stats.evaluated_leaves;
#endif
		return current_evaluation;
	}

	short best_value = result::loss;

	d.beta = beta;
	d.alpha = alpha;
	d.remaining_depth = limit - depth;

	if( got_old_best ) {
		// Not a terminal node, do this check early:
		if( depth >= limit ) {
	#ifdef USE_STATISTICS
			++stats.evaluated_leaves;
	#endif
			return current_evaluation;
		}

		position new_pos = p;
		bool captured = apply_move( new_pos, d.best_move, c );
		unsigned long long new_hash = update_zobrist_hash( p, c, hash, d.best_move );
		short new_eval = evaluate_move( p, c, current_evaluation, d.best_move );
		short value = -step( depth + 1, max_depth, new_pos, new_hash, -new_eval, captured, static_cast<color::type>(1-c), -beta, -alpha );
		if( value > best_value ) {

			best_value = value;

			if( value > alpha ) {
				alpha = value;
			}
			if( alpha >= beta ) {
#ifdef USE_STATISTICS
				++stats.evaluated_intermediate;
#endif
				d.evaluation = alpha;
				store( hash, reinterpret_cast<unsigned char const* const>(&d) );
				return alpha;
			}
		}
	}


	move_info moves[200];
	move_info* pm = moves;
	calculate_moves( p, c, current_evaluation, pm, check );

	if( pm == moves ) {
		ASSERT( !got_old_best || d.remaining_depth == -127 );
		if( check.check ) {
			d.remaining_depth = -127;
			d.evaluation = result::loss;
			store( hash, reinterpret_cast<unsigned char const* const>(&d) );
#ifdef USE_STATISTICS
			++stats.evaluated_leaves;
#endif
			return result::loss + depth;
		}
		else {
			d.remaining_depth = -127;
			d.evaluation = result::draw;
			store( hash, reinterpret_cast<unsigned char const* const>(&d) );
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

	++depth;
	
	d.best_move = moves->m;

	for( move_info const* it  = moves; it != pm; ++it ) {
		position new_pos = p;
		bool captured = apply_move( new_pos, it->m, c );
		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->m );
		short value = -step( depth, max_depth, new_pos, new_hash, -it->evaluation, captured, static_cast<color::type>(1-c), -beta, -alpha );
		if( value > best_value ) {
			best_value = value;

			d.best_move = it->m;

			if( value > alpha ) {
				alpha = value;
			}
		}
		if( alpha >= beta )
			break;
	}

	d.evaluation = alpha;
	store( hash, reinterpret_cast<unsigned char const* const>(&d) );

	return alpha;
}


class processing_thread : public thread
{
public:
	processing_thread( mutex& mtx, condition& cond )
		: mutex_(mtx), cond_(cond), finished_()
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

	void process( position const& p, color::type c, move_info const& m, short max_depth, short alpha, short beta )
	{
		p_ = p;
		c_ = c;
		m_ = m;
		max_depth_ = max_depth;
		alpha_ = alpha;
		beta_ = beta;
		finished_ = false;

		spawn();
	}

	move_info get_move() const { return m_; }

	virtual void onRun();
private:
	mutex& mutex_;
	condition& cond_;

	position p_;
	color::type c_;
	move_info m_;
	short max_depth_;
	short alpha_;
	short beta_;

	bool finished_;
	short result_;
};


void processing_thread::onRun()
{
	position new_pos = p_;
	bool captured = apply_move( new_pos, m_.m, c_ );
	unsigned long long hash = get_zobrist_hash( new_pos, static_cast<color::type>(1-c_) );
	short value = -step( 1, max_depth_, new_pos, hash, -m_.evaluation, captured, static_cast<color::type>(1-c_), -beta_, -alpha_ );

	scoped_lock l( mutex_ );
	result_ = value;
	finished_ = true;
	cond_.signal( l );
}

bool calc( position& p, color::type c, move& m, int& res )
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
	typedef std::multimap<short, move_info> sorted_moves;
	sorted_moves old_sorted;
	for( move_info const* it  = moves; it != pm; ++it ) {
		old_sorted.insert( std::make_pair( it - moves, *it ) );
	}

	unsigned long long start = get_time();

	mutex mtx;

	condition cond;

	std::vector<processing_thread*> threads;
	int thread_count = 6;
	for( int t = 0; t < thread_count; ++t ) {
		threads.push_back( new processing_thread( mtx, cond ) );
	}

	for( int max_depth = 2 + (MAX_DEPTH % 2); max_depth <= MAX_DEPTH; max_depth += 2 )
	{
		short alpha = result::loss;
		short beta = result::win;

		sorted_moves sorted;

		sorted_moves::const_iterator it = old_sorted.begin();
		int evaluated = 0;

		bool abort = false;
		bool done = false;
		while( !done ) {
			unsigned long long now = get_time();
			if( (now - start) > 30000 ) {
				std::cerr << "Triggering search abort due to time limit at depth " << max_depth << std::endl;
				abort = true;
				break;
			}

			while( it != old_sorted.end() && !abort ) {
				int t;
				for( t = 0; t < thread_count; ++t ) {
					if( threads[t]->spawned() ) {
						continue;
					}

					threads[t]->process( p, c, it->second, max_depth, alpha, beta );

					// First one is run on its own to get a somewhat sane lower bound for others to work with.
					if( it++ == old_sorted.begin() ) {
						goto break2;
					}
					break;
				}

				if( t == thread_count ) {
					break;
				}
			}
break2:

			scoped_lock l( mtx );
			cond.wait( l );

			bool all_idle = true;
			for( int t = 0; t < thread_count; ++t ) {
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

				sorted.insert( std::make_pair( -value, threads[t]->get_move() ) );
			}

			if( it == old_sorted.end() && all_idle ) {
				done = true;
			}
		}
		if( abort ) {
			std::cerr << "Aborted with " << evaluated << " moves evaluated at current depth" << std::endl;
		}

		res = alpha;
		if( alpha < result::loss_threshold || alpha > result::win_threshold ) {
			if( max_depth < MAX_DEPTH ) {
				std::cerr << "Early break at " << max_depth << std::endl;
			}
			abort = true;
		}

		if( !sorted.empty() ) {
			sorted.swap( old_sorted );
		}
		if( abort ) {
			break;
		}
	}

	m = old_sorted.begin()->second.m;

	unsigned long long stop = get_time();
	print_stats( start, stop );
	reset_stats();

	return true;
}


