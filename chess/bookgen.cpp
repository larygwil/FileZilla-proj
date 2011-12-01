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
#include <fstream>
#include <list>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>

int const MAX_BOOKSEARCH_DEPTH = 13;

unsigned int const MAX_BOOK_DEPTH = 10;

bool calculate_position( book& b, position const& p, color::type c, seen_positions const& seen, std::vector<move> const& move_history )
{
	short eval = evaluate_fast( p, c );

	move_info moves[200];
	move_info* pm = moves;
	check_map check;
	calc_check_map( p, c, check );
	calculate_moves( p, c, pm, check );
	if( pm == moves ) {
		return true;
	}

	std::vector<book_entry> entries;

	unsigned long long hash = get_zobrist_hash( p );

	context ctx;
	ctx.clock = move_history.size() % 256;
	ctx.seen = seen;

	for( move_info const* it = moves; it != pm; ++it ) {
		position new_pos = p;
		apply_move( new_pos, it->m, c );

		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->m );

		short value;
		if( ctx.seen.is_two_fold( new_hash, 1 ) ) {
			value = 0;
		}
		else {
			ctx.seen.pos[++ctx.seen.root_position] = new_hash;

			pv_entry* pv = ctx.pv_pool.get();

			short new_eval = evaluate_fast( new_pos, c );
			value = -step( (MAX_BOOKSEARCH_DEPTH - 2) * depth_factor, 1, ctx, new_pos, new_hash, -new_eval, static_cast<color::type>(1-c), result::loss, result::win, pv, true );
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
			ctx.seen.pos[++ctx.seen.root_position] = new_hash;
			pv_entry* pv = ctx.pv_pool.get();
			value = -step( MAX_BOOKSEARCH_DEPTH * depth_factor, 1, ctx, new_pos, new_hash, -new_eval, static_cast<color::type>(1-c), result::loss, result::win, pv, true );
			ctx.pv_pool.release(pv);
		}

		entry.search_depth = MAX_BOOKSEARCH_DEPTH;
		entry.forecast = value;

		std::cerr << "F";
	}

	return b.add_entries( move_history, entries );
}


bool update_position( book& b, position const& p, color::type c, seen_positions const& seen, std::vector<move> const& move_history, std::vector<book_entry> const& entries )
{
	unsigned long long hash = get_zobrist_hash( p );

	context ctx;
	ctx.clock = move_history.size() % 256;
	ctx.seen = seen;

	for( std::vector<book_entry>::const_iterator it = entries.begin(); it != entries.end(); ++it ) {
		book_entry entry = *it;

		if( entry.search_depth < MAX_BOOKSEARCH_DEPTH ) {
			position new_pos = p;
			apply_move( new_pos, entry.m, c );

			unsigned long long new_hash = update_zobrist_hash( p, c, hash, entry.m );

			short value;
			if( ctx.seen.is_two_fold( new_hash, 1 ) ) {
				value = 0;
			}
			else {
				ctx.seen.pos[++ctx.seen.root_position] = new_hash;

				short eval = evaluate_fast( new_pos, c );

				pv_entry* pv = ctx.pv_pool.get();
				value = -step( (MAX_BOOKSEARCH_DEPTH) * depth_factor, 1, ctx, new_pos, new_hash, -eval, static_cast<color::type>(1-c), result::loss, result::win, pv, true );
				ctx.pv_pool.release(pv);
			}

			entry.forecast = value;
			entry.search_depth = MAX_BOOKSEARCH_DEPTH;

			b.update_entry( move_history, entry );
			std::cerr << ".";
		}
		else {
			std::cerr << "x";
		}
	}

	return true;
}


void init_book( book& b )
{
	position p;
	init_board(p);

	seen_positions seen;
	seen.pos[0] = get_zobrist_hash( p );

	std::vector<move> move_history;

	if( !calculate_position( b, p, color::white, seen, move_history ) ) {
		std::cerr << "Could not save position" << std::endl;
		exit(1);
	}
}

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


void get_work( book& b, worklist& wl, int max_depth, unsigned int max_width, seen_positions const& seen, std::vector<move> const& move_history, position const& p, color::type c )
{
	std::vector<book_entry> moves = b.get_entries( p, c, move_history );

	unsigned int i = 0;
	for( std::vector<book_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it, ++i ) {
		if( i >= max_width ) {
			break;
		}

		position new_pos = p;
		apply_move( new_pos, it->m, c );

		std::vector<move> child_history = move_history;
		child_history.push_back( it->m );
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
		entries_.clear();

		finished_ = false;

		spawn();
	}

	void process( book_entry_with_position const& w )
	{
		w_ = w.w;
		entries_ = w.entries;

		finished_ = false;

		spawn();
	}

	virtual void onRun();
private:
	book& b_;
	mutex& mutex_;
	condition& cond_;

	work w_;
	std::vector<book_entry> entries_;
	bool finished_;
};

void processing_thread::onRun()
{
	if( entries_.empty() ) {
		calculate_position( b_, w_.p, w_.c, w_.seen, w_.move_history );
	}
	else {
		update_position( b_, w_.p, w_.c, w_.seen, w_.move_history, entries_ );
	}

	scoped_lock l(mutex_);
	finished_ = true;
	cond_.signal(l);
}
}


void go( book& b, position const& p, color::type c, seen_positions const& seen, std::vector<move> const& move_history, unsigned int max_depth, unsigned int max_width )
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
					all_idle = false;
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


void process( book& b )
{
	std::list<work> wl = b.get_unprocessed_positions();
	if( wl.empty() ) {
		return;
	}

	std::cerr << "Got " << wl.size() << " positions to calculate" << std::endl;

	mutex mtx;
	condition cond;

	std::vector<processing_thread*> threads;
	int thread_count = conf.thread_count;
	for( int t = 0; t < thread_count; ++t ) {
		threads.push_back( new processing_thread( b, mtx, cond ) );
	}

	scoped_lock l(mtx);

	bool all_idle = true;

	unsigned long long start = get_time();
	unsigned long long calculated = 0;

	while( true ) {

		while( !wl.empty() ) {

			int t;
			for( t = 0; t < thread_count; ++t ) {
				if( threads[t]->spawned() ) {
					continue;
				}

				work w = wl.front();
				wl.pop_front();
				threads[t]->process( w );
				all_idle = false;
				if( wl.empty() ) {
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
			std::cerr << std::endl << "Remaining work " << wl.size() << " being processed with " << (calculated * 3600) * timer_precision() / (now - start) << " moves/hour" << std::endl;
		}

		if( all_idle && stop ) {
			break;
		}
	}

	if( !stop ) {
		std::cerr << "All done" << std::endl;
	}
}


void update( book& b, int entries_per_pos = 5 )
{
	std::list<book_entry_with_position> wl = b.get_all_entries( entries_per_pos );
	if( wl.empty() ) {
		return;
	}

	int removed_moves = 0;
	int removed_positions = 0;
	{
		std::list<book_entry_with_position>::iterator it = wl.begin();
		while( it != wl.end() ) {
			book_entry_with_position& w = *it;
			std::vector<book_entry>& entries = w.entries;

			for( std::size_t i = 0; i < entries.size(); ) {
				if( entries[i].search_depth >= MAX_BOOKSEARCH_DEPTH ) {
					entries.erase( entries.begin() + i );
					++removed_moves;
				}
				else {
					++i;
				}
			}

			if( entries.empty() ) {
				wl.erase( it++ );
				++removed_positions;
			}
			else {
				++it;
			}
		}
	}

	std::cerr << "Got " << wl.size() << " positions to calculate" << std::endl;
	std::cerr << "Pruned " << removed_moves << " moves and " << removed_positions << " positions which do not need updating" << std::endl;

	mutex mtx;
	condition cond;

	std::vector<processing_thread*> threads;
	int thread_count = conf.thread_count;
	for( int t = 0; t < thread_count; ++t ) {
		threads.push_back( new processing_thread( b, mtx, cond ) );
	}

	scoped_lock l(mtx);

	bool all_idle = true;

	unsigned long long start = get_time();
	unsigned long long calculated = 0;

	while( true ) {

		while( !wl.empty() ) {

			int t;
			for( t = 0; t < thread_count; ++t ) {
				if( threads[t]->spawned() ) {
					continue;
				}

				book_entry_with_position w = wl.front();
				wl.pop_front();
				threads[t]->process( w );
				all_idle = false;
				if( wl.empty() ) {
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
			std::cerr << std::endl << "Remaining work " << wl.size() << " being processed with " << (calculated * 3600) * timer_precision() / (now - start) << " moves/hour" << std::endl;
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


void learnpgn( book& b, std::string const& file )
{
	if( file.empty() ) {
		std::cerr << "No filename given" << std::endl;
	}

	std::ifstream in( file );
	std::string line;

	position p;
	color::type c = color::white;
	std::vector<move> move_history;

	bool valid = true;

	while( std::getline( in, line ) ) {
		if( line.empty() ) {
			continue;
		}

		if( line[0] == '[' ) {
			if( move_history.size() ) {
				std::cerr << ".";
				while( move_history.size() > 20 ) {
					move_history.pop_back();
				}
				b.mark_for_processing( move_history );
			}

			init_board( p );
			c = color::white;
			move_history.clear();
			valid = true;
			continue;
		}

		if( !valid ) {
			continue;
		}

		std::istringstream ss( line );
		std::string token;

		while( ss >> token ) {
			if( token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*" ) {
				valid = false;
				break;
			}

			if( token == "+" ) {
				continue;
			}

			if( std::isdigit(token[0]) ) {
				std::ostringstream os;
				os << (move_history.size() / 2) + 1;
				if( move_history.size() % 2 ) {
					os << "...";
				}
				else {
					os << ".";
				}

				if( token != os.str() ) {
					valid = false;
					std::cerr << "Invalid move number token: " << token <<std::endl;
					std::cerr << "Expected: " << os.str() << std::endl;
					break;
				}

				continue;
			}

			move m;
			if( !parse_move( p, c, token, m ) ) {
				std::cerr << "Invalid move: " << token << std::endl;
				std::cerr << "Line: " << line << std::endl;
				valid = false;
				break;
			}

			apply_move( p, m, c );
			c = static_cast<color::type>(1-c);
			move_history.push_back(m);

		}
	}
	if( move_history.size() ) {
		while( move_history.size() > 20 ) {
			move_history.pop_back();
		}
		b.mark_for_processing( move_history );
	}

}


void run( book& b )
{
	position p;
	init_board( p );

	std::vector<move> move_history;
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
		else if( line == "process" ) {
			process( b );
		}
		else if( line == "size" || line == "stats" ) {

			book_stats stats = b.stats();

			std::stringstream ss;

			ss << "Plies Processed Queued" << std::endl;
			ss << "----------------------" << std::endl;
			for ( auto it : stats.data ) {
				ss << std::setw( 5 ) << it.first;
				ss << std::setw( 10 ) << it.second.processed;
				ss << std::setw( 7 ) << it.second.queued << std::endl;
			}
			ss << "----------------------" << std::endl;
			ss << std::setw( 5 ) << "Total";
			ss << std::setw( 10 ) << stats.total_processed;
			ss << std::setw( 7 ) << stats.total_queued << std::endl;

			std::cout << ss.str();
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
		else if( line.substr( 0, 6 ) == "update" ) {
			int v = 5;
			if( line.size() > 6 ) {
				v = atoi( line.substr( 7 ).c_str() );
			}
			if( v <= 0 ) {
				v = 5;
			}
			update( b, v );
		}
		else if( line.substr( 0, 9 ) == "learnpgn ") {
			learnpgn( b, line.substr( 9 ) );
		}
		else if( !line.empty() ) {
			move m;
			if( parse_move( p, c, line, m ) ) {

				history_entry h;
				h.p = p;
				h.c = c;
				h.m = m;
				history.push_back( h );

				move_history.push_back( m );

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

	//signal( SIGINT, &on_signal );

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
