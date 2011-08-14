#include "book.hpp"
#include "calc.hpp"
#include "config.hpp"
#include "eval.hpp"
#include "hash.hpp"
#include "moves.hpp"
#include "platform.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <deque>
#include <iostream>
#include <map>
#include <sstream>

#include <signal.h>

unsigned int const MAX_BOOK_DEPTH = 10;

typedef std::map<unsigned long long, unsigned long long> TranspositionCache;
TranspositionCache transposition_cache;

struct UnknownTransposition {
	unsigned long long parent;
	int parent_move_index;
};
typedef std::multimap<unsigned long long, UnknownTransposition> UnknownTranspositions;
UnknownTranspositions unknown_transpositions;

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

unsigned long long calculate_position( position const& p, color::type c, int depth, unsigned long long parent_index )
{
	book_entry entry;
	entry.reached_at_depth = depth;
	entry.parent_index = parent_index;

	unsigned long long hash = get_zobrist_hash( p, c );

	short eval = evaluate( p, c );

	move_info moves[200];
	move_info* pm = moves;
	check_map check;
	calc_check_map( p, c, check );
	calculate_moves( p, c, eval, pm, check );

	sorted_moves moves_with_forecast;

	context ctx;
	ctx.max_depth = MAX_DEPTH - 2;
	ctx.quiescence_depth = QUIESCENCE_SEARCH;
	for( move_info const* it = moves; it != pm; ++it ) {
		position new_pos = p;
		bool capture = apply_move( new_pos, it->m, c );

		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->m );
		short value = -step( 1, ctx, new_pos, new_hash, -it->evaluation, capture, static_cast<color::type>(1-c), result::loss, result::win );

		move_entry m;
		m.set_move( it->m );
		m.forecast = value;
		m.full_depth = MAX_DEPTH - 2;
		m.quiescence_depth = QUIESCENCE_SEARCH;
		insert_sorted( moves_with_forecast, m );

		std::cerr << ".";
	}

	sorted_moves moves_with_forecast_fulldepth;

	ctx.max_depth = MAX_DEPTH;

	int fulldepth = 5;
	sorted_moves::const_iterator it;
	for( it = moves_with_forecast.begin(); fulldepth && it != moves_with_forecast.end(); ++it, --fulldepth ) {
		position new_pos = p;
		bool capture = apply_move( new_pos, it->get_move(), c );
		short new_eval = evaluate_move( p, c, eval, it->get_move() );

		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->get_move() );
		short value = -step( 1, ctx, new_pos, new_hash, -new_eval, capture, static_cast<color::type>(1-c), result::loss, result::win );

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

	calculate_position( p, color::white, 0, 0 );
}

struct work {
	int depth;
	position p;
	color::type c;
	unsigned long long index; // position
	int move_index; // move inside position
};

struct worklist {
	worklist() : next_work(), count()
		{}

	bool empty() const { return !count; }

	void clear() {
		count = 0;
		next_work = 0;
		for( unsigned int i = 0; i < MAX_BOOK_DEPTH; ++i ) {
			work_at_depth[i].clear();
		}
	}

	std::vector<work> work_at_depth[MAX_BOOK_DEPTH];
	unsigned int next_work;

	unsigned int count;
};


void get_work( worklist& wl, int max_depth, unsigned int max_width, int depth, unsigned long long index, position const& p, color::type c )
{
	std::vector<move_entry> moves;
	/*book_entry entry = */
	get_entries( index, moves );

	unsigned int i = 0;
	for( std::vector<move_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it, ++i ) {
		if( i >= max_width ) {
			break;
		}

		position new_pos = p;
		apply_move( new_pos, it->get_move(), c );

		if( !it->next_index ) {
			work w;
			w.depth = depth;
			w.index = index;
			w.move_index = it - moves.begin();
			w.p = new_pos;
			w.c = static_cast<color::type>(1-c);
			wl.work_at_depth[depth].push_back(w);
			++wl.count;
		}
		else {
			if( depth + 1 < max_depth ) {
				get_work( wl, max_depth, max_width, depth + 1, it->next_index, new_pos, static_cast<color::type>(1-c) );
			}
		}
	}
}

bool get_next( worklist& wl, work& w )
{
	unsigned int i = wl.next_work;
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
	unsigned long long index = calculate_position( w_.p, w_.c, w_.depth, w_.index );
	book_update_move( w_.index, w_.move_index, index );

	scoped_lock l(mutex_);
	finished_ = true;
	cond_.signal(l);
}
}

void go( position const& p, color::type c, unsigned long long index, unsigned int depth, unsigned int max_depth, unsigned int max_width )
{
	mutex mtx;
	condition cond;

	std::vector<processing_thread*> threads;
	int thread_count = 12;
	for( int t = 0; t < thread_count; ++t ) {
		threads.push_back( new processing_thread( mtx, cond ) );
	}

	max_depth += depth;
	if( max_depth > MAX_BOOK_DEPTH ) {
		max_depth = MAX_BOOK_DEPTH;
	}

	worklist wl;

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
				get_work( wl, max_depth, max_width, depth + 1, index, p, c );
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
}

void print_pos( position const& p, color::type c, std::vector<move_entry> const& moves )
{
	std::cout << "Possible moves:" << std::endl;
	for( std::vector<move_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
		std::cout << move_to_string( p, c, it->get_move() ) << " " << (it->next_index ? "    in book" : "not in book") << " with forecast " << it->forecast << " " << std::endl;
	}
	if( c == color::white ) {
		std::cout << "White to move" << std::endl;
	}
	else {
		std::cout << "Black to move" << std::endl;
	}
}

struct history_entry {
	position p;
	color::type c;
	move m;
	std::string move_string;
	unsigned long long book_index;
};

void print_pos( std::vector<history_entry> const& history, unsigned long long book_index, position const& p, color::type c, std::vector<move_entry> const& moves )
{
	std::stringstream ss;
	for( unsigned int i = 0; i < history.size(); ++i ) {
		if( i ) {
			ss << " ";
		}
		if( !(i%2) ) {
			ss << i / 2 + 1<< ". ";
		}
		else {
			ss << " ";
		}
		ss << history[i].move_string;
	}
	std::string line = ss.str();
	std::string transposition = line_string( book_index );

	std::cout << std::endl;
	std::cout << "Current book index: " << book_index << std::endl;
	std::cout << "Line: " << line << std::endl;
	if( line != transposition ) {
		std::cout << "Transposition of: " << line_string( book_index ) << std::endl;
	}
	print_pos( p, c, moves );
}

void cleanup_book()
{
	/*
	 * This nifty function reads in the entire opening book breath-first,
	 * and builds the transposition cache. When encountering a so-far-unknown
	 * transposition, it updates the book.
	 * It also fixes the depth information.
	 */

restart:
	// Phase 1: Vacuum
	vacuum_book();

	// Phase 2: Fix depths
	unsigned long long fixed_entries = fix_depth( 0, 0 );
	if( fixed_entries ) {
		std::cout << "Warning: Fixed depth of " << fixed_entries << " entries." << std::endl;
	}

	transposition_cache.clear();
	unknown_transpositions.clear();

	bool vacuum_and_restart = false;

	unsigned long long merged = 0;

	unsigned long long transpositions = 0;

	// Breath-first
	struct TranspositionWork {
		position p;
		color::type c;
		unsigned long long index;
	};
	std::deque<TranspositionWork> toVisit;

	{
		// Get started
		TranspositionWork w;
		init_board( w.p );
		w.c = color::white;
		w.index = 0;

		transposition_cache[get_zobrist_hash( w.p, color::white )] = 0;
		toVisit.push_back( w );
	}

	// Phase 3: Merge calculated transpositions
	while( !toVisit.empty() ) {
		TranspositionWork const w = toVisit.front();
		toVisit.pop_front();

		std::vector<move_entry> moves;
		get_entries( w.index, moves );

		for( std::vector<move_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
			TranspositionWork new_work;
			new_work.c = static_cast<color::type>(1-w.c);
			new_work.p = w.p;
			new_work.index = it->next_index;
			apply_move( new_work.p, it->get_move(), new_work.c );
			unsigned long long hash = get_zobrist_hash( new_work.p, new_work.c );

			if( !it->next_index ) {
				UnknownTransposition ut;
				ut.parent = w.index;
				ut.parent_move_index = it - moves.begin();
				unknown_transpositions.insert( std::make_pair(hash, ut) );
				continue;
			}

			new_work.index = it->next_index;

			TranspositionCache::const_iterator hash_it = transposition_cache.find(hash);
			if( hash_it != transposition_cache.end() ) {
				++transpositions;

				if( new_work.index == hash_it->second ) {
					continue;
				}

				std::cout << "Warning: Found unhandled transposition, merging positions." << std::endl;
				vacuum_and_restart = true;

				std::vector<move_entry> old_moves;
				std::vector<move_entry> new_moves;

				book_entry old_bentry = get_entries( hash_it->second, old_moves );
				book_entry new_bentry = get_entries( new_work.index, new_moves );

				if( new_bentry.reached_at_depth < old_bentry.reached_at_depth ) {
					std::cerr << "Corrupted book! Old book entry with depth " << old_bentry.reached_at_depth << " with new entry having lower depth of " << new_bentry.reached_at_depth << std::endl;
					exit(1);
				}

				if( new_moves.size() != old_moves.size() ) {
					std::cerr << "Corrupted book! Number of moves at two transposition sites differ: " << new_moves.size() << " at " << new_work.index << ", " << old_moves.size() << " at " << hash_it->second << std::endl;
					std::cerr << line_string( hash_it->second ) << std::endl;
					std::cerr << line_string( new_work.index ) << std::endl;
					exit(1);
				}

				for( unsigned int j = 0; j < old_moves.size(); ++j ) {
					unsigned int i;
					for( i = 0; i < new_moves.size(); ++i ) {
						if( old_moves[j].get_move() == new_moves[i].get_move() ) {
							if( old_moves[j].next_index && !new_moves[i].next_index ) {
								book_update_move( new_work.index, i, old_moves[j].next_index );
								++merged;
							}
							else if( !old_moves[j].next_index && new_moves[i].next_index ) {
								book_update_move( hash_it->second, j, new_moves[i].next_index );
								++merged;
							}
						}
						break;
					}
					if( i == new_moves.size() ) {
						std::cerr << "Corrupted book! Different moves at " << new_work.index << " and " << hash_it->second << std::endl;
						exit(1);
					}
				}

				book_update_move( w.index, it - moves.begin(), hash_it->second );
			}
			else {
				transposition_cache[ hash ] = it->next_index;

				book_entry new_bentry = get_entry( new_work.index );
				if( new_bentry.parent_index != w.index ) {
					std::cerr << "Warning: updating parent of " << new_work.index << " from " << new_bentry.parent_index << " to " << w.index << std::endl;
					set_parent( new_work.index, w.index );
				}
			}
			toVisit.push_back( new_work );
		}
	}

	std::cout << "Got " << transpositions << " transpositions." << std::endl;

	if( merged ) {
		std::cerr << "Merged " << merged << " moves." << std::endl;
	}
	if( vacuum_and_restart ) {
		goto restart;
	}

	// Phase 4: Merge calculated and unknown positions
	unsigned long long added_transpositions = 0;
	for( TranspositionCache::const_iterator it = transposition_cache.begin(); it != transposition_cache.end(); ++it ) {

		for( UnknownTranspositions::const_iterator uit = unknown_transpositions.find( it->first ); uit != unknown_transpositions.end() && uit->first == it->first; ++uit ) {
			std::vector<move_entry> moves;
			get_entries( uit->second.parent, moves );
			if( moves[uit->second.parent_move_index].next_index ) {
				std::cerr << "Corrupted book! Unknown entry with an index." << std::endl;
				exit(1);
			}
			++added_transpositions;

			book_update_move( uit->second.parent, uit->second.parent_move_index, it->second );
		}
	}
	if( added_transpositions ) {
		std::cerr << "Warning: Discovered " << added_transpositions << " new transposition(s)." << std::endl;
		goto restart;
	}
}

void run()
{
	position p;
	init_board( p );
	unsigned long long book_index = 0;
	color::type c = color::white;

	std::vector<move_entry> moves;
	book_entry bentry = get_entries( book_index, moves );

	std::cout << std::endl;
	print_pos( p, c, moves );

	int depth = 0;

	unsigned int max_depth = std::min(4u, MAX_BOOK_DEPTH);
	unsigned int max_width = 2;

	std::vector<history_entry> history;

	while( true ) {
		std::string line;

		std::cout << "Move: ";
		std::getline( std::cin, line );
		if( !std::cin ) {
			break;
		}
		if( line == "go" ) {
			go( p, c, book_index, depth, max_depth, max_width );
			return;
		}
		else if( line == "cleanup" ) {
			cleanup_book();
		}
		else if( line == "stats" ) {
			print_book_stats();
		}
		else if( line.substr( 0, 11 ) == "book_depth " ) {
			int v = atoi( line.substr( 11 ).c_str() );
			if( v <= 0 || v > static_cast<int>(MAX_BOOK_DEPTH) ) {
				std::cerr << "Invalid depth: " << v << std::endl;
			}
			else {
				max_depth = v;
				std::cout << "Book depth set to " << v << std::endl;
			}
		}
		else if( line.substr( 0, 11 ) == "book_width " ) {
			int v = atoi( line.substr( 11 ).c_str() );
			if( v <= 0 ) {
				std::cerr << "Invalid width: " << v << std::endl;
			}
			else {
				max_width = v;
				std::cout << "Book width set to " << v << std::endl;
			}
		}
		else if( line == "back" ) {
			if( !book_index ) {
				std::cerr << "Already at top" << std::endl;
			}
			else {
				history_entry h = history.back();
				history.pop_back();
				book_index = h.book_index;
				p = h.p;
				c = h.c;
				bentry = get_entries( book_index, moves );
				print_pos( history, book_index, p, c, moves );
			}
		}
		else if( !line.empty() ) {
			move m;
			if( parse_move( p, c, line, m ) ) {

				++depth;

				history_entry h;
				h.p = p;
				h.c = c;
				h.m = m;
				h.move_string = move_to_string( p, c, m );
				h.book_index = book_index;
				history.push_back( h );

				apply_move( p, m, c );
				c = static_cast<color::type>( 1 - c );

				bool in_book = false;
				for( std::vector<move_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
					if( it->get_move() == m ) {
						if( it->next_index ) {
							book_index = it->next_index;
						}
						else {
							std::cout << "Position not in book, calculating..." << std::endl;
							unsigned long long new_index = calculate_position( p, c, depth, book_index );
							book_update_move( book_index, it - moves.begin(), new_index );
							book_index = new_index;
						}
						in_book = true;
						break;
					}
				}
				if( !in_book ) {
					std::cerr << "Position not in book!" << std::endl;
					exit(1);
				}

				bentry = get_entries( book_index, moves );
				print_pos( history, book_index, p, c, moves );
			}
		}
	}
}


int main( int argc, char const* argv[] )
{
	std::cout << "Initializing" << std::endl;

	conf.init( argc, argv );

	std::string book_dir;
	std::string self = argv[0];
	if( self.rfind('/') != std::string::npos ) {
		book_dir = self.substr( 0, self.rfind('/') + 1 );
	}

	signal( SIGINT, &on_signal );

	console_init();

	init_random( 1234 );
	init_zobrist_tables();

	init_hash( conf.memory, sizeof(step_data) );

	std::cout << "Opening book" << std::endl;

	if( !open_book( book_dir ) ) {
		std::cerr << "Cound not open opening book" << std::endl;
		return 1;
	}

	if( needs_init() ) {
		init_book();
	}

	std::cout << "Ready" << std::endl;

	run();

	return 0;
}
