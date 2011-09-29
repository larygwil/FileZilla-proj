#include "book.hpp"
#include "calc.hpp"
#include "config.hpp"
#include "eval.hpp"
#include "hash.hpp"
#include "moves.hpp"
#include "pawn_structure_hash_table.hpp"
#include "platform.hpp"
#include "pvlist.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <deque>
#include <iostream>
#include <map>
#include <sstream>

#include <signal.h>

int const MAX_BOOKSEARCH_DEPTH = 12;

unsigned int const MAX_BOOK_DEPTH = 10;

bool calculate_position( book& b, position const& p, color::type c, seen_positions const& seen, std::vector<std::string> const& move_history )
{
	short eval = evaluate_fast( p, c );

	move_info moves[200];
	move_info* pm = moves;
	check_map check;
	calc_check_map( p, c, check );
	calculate_moves( p, c, eval, pm, check, killer_moves() );
	if( pm == moves ) {
		return true;
	}

	std::vector<book_entry> entries;

	unsigned long long hash = get_zobrist_hash( p );

	context ctx;
	ctx.max_depth = MAX_BOOKSEARCH_DEPTH - 2;
	ctx.quiescence_depth = conf.quiescence_depth;
	ctx.clock = move_history.size() % 256;
	ctx.seen = seen;

	for( move_info const* it = moves; it != pm; ++it ) {
		position new_pos = p;
		apply_move( new_pos, *it, c );

		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->m );

		short value;
		if( ctx.seen.is_two_fold( new_hash, 1 ) ) {
			value = 0;
		}
		else {
			ctx.seen.pos[ctx.seen.root_position + 1] = new_hash;

			pv_entry* pv = ctx.pv_pool.get();
			value = -step(ctx.max_depth, 1, ctx, new_pos, new_hash, -it->evaluation, static_cast<color::type>(1-c), result::loss, result::win, pv, true );
			ctx.pv_pool.release(pv);
		}

		book_entry entry;
		entry.forecast = value;
		entry.m = it->m;
		entry.search_depth = MAX_BOOKSEARCH_DEPTH - 2;
		entries.push_back( entry );

		std::cerr << ".";
	}

	std::sort( entries.begin(), entries.end() );

	ctx.max_depth = MAX_BOOKSEARCH_DEPTH;

	std::size_t fulldepth = 5;
	if( entries.size() < 5 ) {
		fulldepth = entries.size();
	}

	for( std::size_t i = 0; i < fulldepth; ++i ) {
		book_entry& entry = entries[i];

		position new_pos = p;
		apply_move( new_pos, entry.m, c );
		position::pawn_structure pawns;
		short new_eval = evaluate_move( p, c, eval, entry.m, pawns );

		unsigned long long new_hash = update_zobrist_hash( p, c, hash, entry.m );

		short value;
		if( ctx.seen.is_two_fold( new_hash, 1 ) ) {
			value = 0;
		}
		else {
			ctx.seen.pos[ctx.seen.root_position + 1] = new_hash;
			pv_entry* pv = ctx.pv_pool.get();
			value = -step( ctx.max_depth, 1, ctx, new_pos, new_hash, -new_eval, static_cast<color::type>(1-c), result::loss, result::win, pv, true );
			ctx.pv_pool.release(pv);
		}

		entry.search_depth = MAX_BOOKSEARCH_DEPTH;
		entry.forecast = value;

		std::cerr << "F";
	}

	return b.add_entries( move_history, entries );
}


void init_book( book& b )
{
	position p;
	init_board(p);

	seen_positions seen;
	seen.pos[0] = get_zobrist_hash( p );

	std::vector<std::string> move_history;

	if( !calculate_position( b, p, color::white, seen, move_history ) ) {
		std::cerr << "Could not save position" << std::endl;
		exit(1);
	}
}

struct work {
	std::vector<std::string> move_history;
	seen_positions seen;
	position p;
	color::type c;
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


void get_work( book& b, worklist& wl, int max_depth, unsigned int max_width, seen_positions const& seen, std::vector<std::string> const& move_history, position const& p, color::type c )
{
	std::vector<book_entry> moves = b.get_entries( p, c, move_history );

	unsigned int i = 0;
	for( std::vector<book_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it, ++i ) {
		if( i >= max_width ) {
			break;
		}

		position new_pos = p;
		apply_move( new_pos, it->m, c );

		std::vector<std::string> child_history = move_history;
		child_history.push_back( move_to_source_target_string( it->m ) );
		std::vector<book_entry> child_moves = b.get_entries( new_pos, static_cast<color::type>(1-c), child_history );

		seen_positions child_seen = seen;
		child_seen.pos[++child_seen.root_position] = get_zobrist_hash( new_pos );

		if( child_moves.empty() ) {
			work w;
			w.p = new_pos;
			w.c = static_cast<color::type>(1-c);
			w.move_history = child_history;
			w.seen = child_seen;
			wl.work_at_depth[child_history.size()].push_back(w);
			++wl.count;
		}
		else if( child_history.size() < static_cast<std::size_t>(max_depth) ) {
			get_work( b, wl, max_depth, max_width, child_seen, child_history, new_pos, static_cast<color::type>(1-c) );
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
	processing_thread( book& b, mutex& mtx, condition& cond )
		: b_(b), mutex_(mtx), cond_(cond), finished_()
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
	book& b_;
	mutex& mutex_;
	condition& cond_;

	work w_;
	bool finished_;
};

void processing_thread::onRun()
{
	calculate_position( b_, w_.p, w_.c, w_.seen, w_.move_history );

	scoped_lock l(mutex_);
	finished_ = true;
	cond_.signal(l);
}
}

void go( book& b, position const& p, color::type c, seen_positions const& seen, std::vector<std::string> const& move_history, unsigned int max_depth, unsigned int max_width )
{
	mutex mtx;
	condition cond;

	std::vector<processing_thread*> threads;
	int thread_count = conf.thread_count;
	for( int t = 0; t < thread_count; ++t ) {
		threads.push_back( new processing_thread( b, mtx, cond ) );
	}

	max_depth += move_history.size();
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
				get_work( b, wl, max_depth, max_width, seen, move_history, p, c );
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
			std::cerr << std::endl << "Remaining work " << wl.count << " being processed with " << (calculated * 3600) * timer_precision() / (now - start) << " moves/hour" << std::endl;
		}

		if( all_idle && stop ) {
			break;
		}
	}

	if( !stop ) {
		std::cerr << "All done" << std::endl;
	}
}

void print_pos( position const& p, color::type c, std::vector<book_entry> const& moves )
{
	std::cout << "Possible moves:" << std::endl;
	for( std::vector<book_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
		std::cout << move_to_string( p, c, it->m ) << " with forecast " << it->forecast << " (" << static_cast<int>(it->search_depth) << ")" << std::endl;
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
};

void print_pos( std::vector<history_entry> const& history, position const& p, color::type c, std::vector<book_entry> const& moves )
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
		ss << move_to_string(history[i].p, history[i].c, history[i].m );
	}
	std::string line = ss.str();

	std::cout << std::endl;
	std::cout << "Line: " << line << std::endl;
	print_pos( p, c, moves );
}


void run( book& b )
{
	position p;
	init_board( p );

	std::vector<std::string> move_history;
	std::vector<history_entry> history;

	seen_positions seen;
	seen.pos[0] = get_zobrist_hash( p );

	color::type c = color::white;

	std::vector<book_entry> moves = b.get_entries( p, c, move_history );

	std::cout << std::endl;
	print_pos( p, c, moves );

	unsigned int max_depth = std::min(4u, MAX_BOOK_DEPTH);
	unsigned int max_width = 2;


	while( true ) {
		std::string line;

		std::cout << "Move: ";
		std::getline( std::cin, line );
		if( !std::cin ) {
			break;
		}
		if( line == "go" ) {
			go( b, p, c, seen, move_history, max_depth, max_width );
			return;
		}
		else if( line == "size" ) {
			std::cout << "Distinct entries: " << b.size() << std::endl;
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
			if( history.empty() ) {
				std::cerr << "Already at top" << std::endl;
			}
			else {
				history_entry h = history.back();
				move_history.pop_back();
				--seen.root_position;
				history.pop_back();
				p = h.p;
				c = h.c;

				std::vector<book_entry> moves = b.get_entries( p, c, move_history );
				print_pos( history, p, c, moves );
			}
		}
		else if( !line.empty() ) {
			move m;
			if( parse_move( p, c, line, m ) ) {

				history_entry h;
				h.p = p;
				h.c = c;
				h.m = m;
				history.push_back( h );

				move_history.push_back( move_to_source_target_string( m ) );

				apply_move( p, m, c );
				c = static_cast<color::type>( 1 - c );

				seen.pos[++seen.root_position] = get_zobrist_hash( p );

				std::vector<book_entry> entries = b.get_entries( p, c, move_history );
				if( entries.empty() ) {
					std::cout << "Position not in book, calculating..." << std::endl;

					if( !calculate_position( b, p, c, seen, move_history ) ) {
						std::cerr << "Failed to calculate position" << std::endl;
						exit(1);
					}

					entries = b.get_entries( p, c, move_history );
				}

				if( entries.empty() ) {
					std::cout << "Aborting, must be final state..." << std::endl;
					exit(1);
				}

				print_pos( history, p, c, entries );
			}
		}
	}
}

const int PAWN_HASH_TABLE_SIZE = 10;

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

	transposition_table.init( conf.memory );

	pawn_hash_table.init( PAWN_HASH_TABLE_SIZE );

	std::cout << "Opening book" << std::endl;

	book b( book_dir );
	if( !b.is_open() ) {
		std::cerr << "Cound not open opening book" << std::endl;
		return 1;
	}

	if( !b.size() ) {
		init_book( b );
	}

	std::cout << "Ready" << std::endl;

	run( b );

	return 0;
}
