#include "book.hpp"
#include "calc.hpp"
#include "eval.hpp"
#include "hash.hpp"
#include "moves.hpp"
#include "platform.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <signal.h>
#include <iostream>

int const MAX_BOOK_DEPTH = 10;

namespace {
typedef std::vector<move_entry> sorted_moves;
void insert_sorted( sorted_moves& moves, move_entry const& m )
{
	sorted_moves::iterator it = moves.begin();
	while( it != moves.end() && *it >= m ) {
		++it;
	}
	moves.insert( it, m );
}
}

unsigned long long calculate_position( position const& p, color::type c, int depth )
{
	book_entry entry;
	entry.reached_at_depth = depth;

	unsigned long long hash = get_zobrist_hash( p, c );

	short eval = evaluate( p, c );

	move_info moves[200];
	move_info* pm = moves;
	check_map check;
	calc_check_map( p, c, check );
	calculate_moves( p, c, eval, pm, check );

	sorted_moves moves_with_forecast;

	for( move_info const* it = moves; it != pm; ++it ) {
		position new_pos = p;
		bool capture = apply_move( new_pos, it->m, c );

		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->m );
		short value = -step( 1, MAX_DEPTH - 2, new_pos, new_hash, -it->evaluation, capture, static_cast<color::type>(1-c), result::loss, result::win );

		move_entry m;
		m.set_move( it->m );
		m.forecast = value;
		m.full_depth = MAX_DEPTH - 2;
		m.quiescence_depth = QUIESCENCE_SEARCH;
		insert_sorted( moves_with_forecast, m );

		std::cerr << ".";
	}

	sorted_moves moves_with_forecast_fulldepth;

	int fulldepth = 5;
	sorted_moves::const_iterator it;
	for( it = moves_with_forecast.begin(); fulldepth && it != moves_with_forecast.end(); ++it, --fulldepth ) {
		position new_pos = p;
		bool capture = apply_move( new_pos, it->get_move(), c );
		short new_eval = evaluate_move( p, c, eval, it->get_move() );

		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->get_move() );
		short value = -step( 1, MAX_DEPTH, new_pos, new_hash, -new_eval, capture, static_cast<color::type>(1-c), result::loss, result::win );

		move_entry m;
		m.set_move( it->get_move() );
		m.forecast = value;
		m.full_depth = MAX_DEPTH;
		m.quiescence_depth = QUIESCENCE_SEARCH;
		insert_sorted( moves_with_forecast_fulldepth, m );

		std::cerr << "F";
	}
	for( ; it != moves_with_forecast.end(); ++it ) {
		insert_sorted( moves_with_forecast_fulldepth, *it );
	}

	return book_add_entry( entry, moves_with_forecast_fulldepth );
}


void init_book()
{
	position p;
	init_board(p);

	calculate_position( p, color::white, 0 );
}

struct work {
	int depth;
	position p;
	color::type c;
	unsigned long long index; // position
	int move_index; // move inside position
};

struct worklist {
	bool empty() const { return !count; }

	std::vector<work> work_at_depth[MAX_BOOK_DEPTH];
	int next_work;

	int count;
};


void get_work( worklist& wl, int max_depth, int max_width, int depth, unsigned long long index, position const& p, color::type c )
{
	std::vector<move_entry> moves;
	/*book_entry entry = */
	get_entries( index, moves );

	unsigned char added_at_pos = 0;
	for( std::vector<move_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
		position new_pos = p;
		apply_move( new_pos, it->get_move(), c );

		if( !it->next_index ) {

			if( added_at_pos < std::min(5, static_cast<int>(moves.size()/5 + 1)) ) {
				++added_at_pos;
				work w;
				w.depth = depth;
				w.index = index;
				w.move_index = it - moves.begin();
				w.p = new_pos;
				w.c = static_cast<color::type>(1-c);
				wl.work_at_depth[depth].push_back(w);
				++wl.count;
			}
		}
		else {
			++added_at_pos;
			if( depth + 1 < max_depth ) {
				get_work( wl, max_depth, max_width, depth + 1, it->next_index, new_pos, static_cast<color::type>(1-c) );
			}
		}
	}
}


void get_work( worklist& wl, int max_depth, int max_width )
{
	wl.next_work = 0;
	wl.count = 0;

	position p;
	init_board( p );

	get_work( wl, max_depth, max_width, 1, 0, p, color::white );
}

bool get_next( worklist& wl, work& w )
{
	int i = wl.next_work;
	do {
		if( !wl.work_at_depth[i].empty() ) {
			w = wl.work_at_depth[i].front();
			wl.work_at_depth[i].erase( wl.work_at_depth[i].begin() );

			if( ++wl.next_work == MAX_BOOK_DEPTH ) {
				wl.next_work = 0;
			}
			--wl.count;
			return true;
		}
		if( ++i == MAX_BOOK_DEPTH ) {
			i = 0;
		}
	} while( i != wl.next_work );

	if( !wl.work_at_depth[i].empty() ) {
		w = wl.work_at_depth[i].front();
		wl.work_at_depth[i].erase( wl.work_at_depth[i].begin() );

		if( ++wl.next_work == MAX_BOOK_DEPTH ) {
			wl.next_work = 0;
		}
		--wl.count;
		return true;
	}

	return false;
}

volatile bool stop = false;


extern "C" void on_signal(int)
{
	stop = true;
}

namespace {
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

	void process( work const& w )
	{
		w_ = w;
		finished_ = false;

		spawn();
	}

	virtual void onRun();
private:
	mutex& mutex_;
	condition& cond_;

	work w_;
	bool finished_;
};

void processing_thread::onRun()
{
	unsigned long long index = calculate_position( w_.p, w_.c, w_.depth );
	book_update_move( w_.index, w_.move_index, index );

	scoped_lock l(mutex_);
	finished_ = true;
	cond_.signal(l);
}
}

int main( int argc, char const* argv[] )
{
	std::string book_dir;
	std::string self = argv[0];
	if( self.rfind('/') != std::string::npos ) {
		book_dir = self.substr( 0, self.rfind('/') + 1 );
	}

	signal( SIGINT, &on_signal );

	console_init();

	init_random( 1234 );
	init_zobrist_tables();

	init_hash( 3*2048, sizeof(step_data) );

	if( !open_book( book_dir ) ) {
		std::cerr << "Cound not open opening book" << std::endl;
		return 1;
	}

	if( needs_init() ) {
		init_book();
	}

	int max_depth = std::min(4, MAX_BOOK_DEPTH);
	int max_width = 2;

	mutex mtx;
	condition cond;

	std::vector<processing_thread*> threads;
	int thread_count = 12;
	for( int t = 0; t < thread_count; ++t ) {
		threads.push_back( new processing_thread( mtx, cond ) );
	}

	worklist wl;
	wl.count = 0;

	scoped_lock l(mtx);

	bool all_idle = true;

	unsigned long long start = get_time();
	unsigned long long calculated = 0;

	while( true ) {

		while( !stop ) {

			if( wl.empty() && !all_idle ) {
				break;
			}

			while( wl.empty() ) {
				get_work( wl, max_depth, max_width );
				if( wl.empty() ) {
					if( max_depth < MAX_BOOK_DEPTH ) {
						++max_depth;
					}
					else {
						break;
					}
				}
				else {
					std::cerr << std::endl << "Created worklist with " << wl.count << " positions to evaluate" << std::endl;
				}
			}

			int t;
			for( t = 0; t < thread_count; ++t ) {
				if( threads[t]->spawned() ) {
					continue;
				}

				work w;
				if( get_next( wl, w ) ) {
					threads[t]->process( w );
					all_idle = false;
				}
				else {
					break;
				}
			}
			if( t == thread_count ) {
				break;
			}
		}
		if( all_idle ) {
			break;
		}

		cond.wait( l );

		all_idle = true;
		for( int t = 0; t < thread_count; ++t ) {
			if( !threads[t]->spawned() ) {
				continue;
			}

			if( !threads[t]->finished() ) {
				all_idle = false;
				continue;
			}

			threads[t]->join();

			++calculated;
			unsigned long long now = get_time();
			std::cerr << std::endl << "Remaining work " << wl.count << " being processed with " << (calculated * 3600 * 1000) / (now - start) << " moves/hour" << std::endl;
		}

		if( all_idle && stop ) {
			break;
		}
	}

	if( !stop ) {
		std::cerr << "All done" << std::endl;
	}

	return 0;
}
