#include "book.hpp"
#include "calc.hpp"
#include "config.hpp"
#include "eval.hpp"
#include "eval_values.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "magic.hpp"
#include "moves.hpp"
#include "pawn_structure_hash_table.hpp"
#include "pgn.hpp"
#include "pvlist.hpp"
#include "random.hpp"
#include "util/mutex.hpp"
#include "util/string.hpp"
#include "util/thread.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <deque>
#include <fstream>
#include <list>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <sstream>

#include <ctype.h>

int const MAX_BOOKSEARCH_DEPTH = 17;

unsigned int const MAX_BOOK_DEPTH = 10;

bool deepen_move( book& b, position const& p, seen_positions const& seen, std::vector<move> const& move_history, move const& m )
{
	int depth = MAX_BOOKSEARCH_DEPTH;

	{
		std::vector<book_entry> entries = b.get_entries( p, move_history );
		for( std::vector<book_entry>::const_iterator it = entries.begin(); it != entries.end(); ++it ) {
			if( it->m != m ) {
				continue;
			}
			if( it->search_depth >= MAX_BOOKSEARCH_DEPTH ) {
				depth = it->search_depth + 1;
			}
		}
	}

	context ctx;
	ctx.clock = move_history.size() % 256;
	ctx.seen = seen;

	position new_pos = p;
	apply_move( new_pos, m );

	uint64_t new_hash = get_zobrist_hash( new_pos );
	short value;
	if( ctx.seen.is_two_fold( new_hash, 1 ) ) {
		value = 0;
	}
	else {
		ctx.seen.push_root( new_hash );

		pv_entry* pv = ctx.pv_pool.get();

		check_map check( new_pos );

		value = -step( depth * depth_factor + MAX_QDEPTH, 1, ctx, new_pos, new_hash, check, result::loss, result::win, pv, true );
		ctx.pv_pool.release(pv);
	}

	book_entry e;
	e.forecast = value;
	e.m = m;
	e.search_depth = depth;
	b.update_entry( move_history, e );

	return true;
}


bool calculate_position( book& b, position const& p, seen_positions const& seen, std::vector<move> const& move_history )
{
	move_info moves[200];
	move_info* pm = moves;
	check_map check( p );
	calculate_moves( p, pm, check );
	if( pm == moves ) {
		return true;
	}

	std::vector<book_entry> entries;

	uint64_t const hash = get_zobrist_hash( p );

	for( move_info const* it = moves; it != pm; ++it ) {
		context ctx;
		ctx.clock = move_history.size() % 256;
		ctx.seen = seen;

		position new_pos = p;
		apply_move( new_pos, it->m );

		uint64_t new_hash = update_zobrist_hash( p, hash, it->m );

		short value;
		if( ctx.seen.is_two_fold( new_hash, 1 ) ) {
			value = 0;
		}
		else {
			ctx.seen.push_root( new_hash );

			pv_entry* pv = ctx.pv_pool.get();

			check_map check( new_pos );

			value = -step( (MAX_BOOKSEARCH_DEPTH - 2) * depth_factor + MAX_QDEPTH, 1, ctx, new_pos, new_hash, check, result::loss, result::win, pv, true );
			ctx.pv_pool.release(pv);
		}

		book_entry entry;
		entry.forecast = value;
		entry.m = it->m;
		entry.search_depth = MAX_BOOKSEARCH_DEPTH - 2;
		entries.push_back( entry );

		std::cerr << ".";
	}

	std::size_t fulldepth = 5;
	if( entries.size() < 5 ) {
		fulldepth = entries.size();
	}

	bool done = false;
	while( !done ) {
		done = true;

		std::sort( entries.begin(), entries.end() );

		for( std::size_t i = 0; i < fulldepth; ++i ) {
			book_entry& entry = entries[i];
			if( entry.search_depth == MAX_BOOKSEARCH_DEPTH ) {
				continue;
			}

			done = false;

			context ctx;
			ctx.clock = move_history.size() % 256;
			ctx.seen = seen;

			position new_pos = p;
			apply_move( new_pos, entry.m );

			uint64_t new_hash = update_zobrist_hash( p, hash, entry.m );

			short value;
			if( ctx.seen.is_two_fold( new_hash, 1 ) ) {
				value = 0;
			}
			else {
				ctx.seen.push_root( new_hash );
				pv_entry* pv = ctx.pv_pool.get();

				check_map check( new_pos );

				value = -step( MAX_BOOKSEARCH_DEPTH * depth_factor + MAX_QDEPTH, 1, ctx, new_pos, new_hash, check, result::loss, result::win, pv, true );
				ctx.pv_pool.release(pv);
			}

			entry.search_depth = MAX_BOOKSEARCH_DEPTH;
			entry.forecast = value;

			std::cerr << "F";
		}
	}

	return b.add_entries( move_history, entries );
}


bool update_position( book& b, position const& p, seen_positions const& seen, std::vector<move> const& move_history, std::vector<book_entry> const& entries )
{
	uint64_t hash = get_zobrist_hash( p );

	for( std::vector<book_entry>::const_iterator it = entries.begin(); it != entries.end(); ++it ) {
		book_entry entry = *it;

		if( entry.search_depth < MAX_BOOKSEARCH_DEPTH || entry.eval_version < eval_version ) {

			int new_depth;
			if( entry.eval_version < eval_version) {
				if( entry.search_depth <= MAX_BOOKSEARCH_DEPTH ) {
					new_depth = MAX_BOOKSEARCH_DEPTH;
				}
				else {
					new_depth = entry.search_depth;
				}
			}
			else {
				new_depth = MAX_BOOKSEARCH_DEPTH;
			}

			context ctx;
			ctx.clock = move_history.size() % 256;
			ctx.seen = seen;

			position new_pos = p;
			apply_move( new_pos, entry.m );

			uint64_t new_hash = update_zobrist_hash( p, hash, entry.m );

			short value;
			if( ctx.seen.is_two_fold( new_hash, 1 ) ) {
				value = 0;
			}
			else {
				ctx.seen.push_root( new_hash );

				check_map check( new_pos );

				pv_entry* pv = ctx.pv_pool.get();
				value = -step( new_depth * depth_factor + MAX_QDEPTH, 1, ctx, new_pos, new_hash, check, result::loss, result::win, pv, true );
				ctx.pv_pool.release(pv);
			}

			std::cerr << entry.forecast << " d" << static_cast<int>(entry.search_depth) << " v" << static_cast<int>(entry.eval_version) << " -> " << value << " d" << new_depth << " " << move_history.size() << " " << position_to_fen_noclock( p ) << " " << move_to_san( p, entry.m ) << std::endl;

			entry.forecast = value;
			entry.search_depth = new_depth;

			b.update_entry( move_history, entry );
		}
	}

	return true;
}


void init_book( book& b )
{
	position p;

	seen_positions seen( get_zobrist_hash( p ) );

	std::vector<move> move_history;

	if( !calculate_position( b, p, seen, move_history ) ) {
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


void get_work( book& b, worklist& wl, int max_depth, unsigned int max_width, seen_positions const& seen, std::vector<move> const& move_history, position const& p )
{
	std::vector<book_entry> moves = b.get_entries( p, move_history );

	unsigned int i = 0;
	for( std::vector<book_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it, ++i ) {
		if( i >= max_width ) {
			break;
		}

		position new_pos = p;
		apply_move( new_pos, it->m );

		std::vector<move> child_history = move_history;
		child_history.push_back( it->m );
		std::vector<book_entry> child_moves = b.get_entries( new_pos, child_history );

		seen_positions child_seen = seen;
		child_seen.push_root( get_zobrist_hash( new_pos ) );

		if( child_moves.empty() ) {
			work w;
			w.p = new_pos;
			w.move_history = child_history;
			w.seen = child_seen;
			wl.work_at_depth[child_history.size()].push_back(w);
			++wl.count;
		}
		else if( child_history.size() < static_cast<std::size_t>(max_depth) ) {
			get_work( b, wl, max_depth, max_width, child_seen, child_history, new_pos );
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
		calculate_position( b_, w_.p, w_.seen, w_.move_history );
	}
	else {
		update_position( b_, w_.p, w_.seen, w_.move_history, entries_ );
	}

	scoped_lock l(mutex_);
	finished_ = true;
	cond_.signal(l);
}
}


void go( book& b, position const& p, seen_positions const& seen, std::vector<move> const& move_history, unsigned int max_depth, unsigned int max_width )
{
	mutex mtx;
	condition cond;

	std::vector<processing_thread*> threads;
	int thread_count = conf.thread_count;
	for( int t = 0; t < thread_count; ++t ) {
		threads.push_back( new processing_thread( b, mtx, cond ) );
	}

	max_depth += static_cast<unsigned int>(move_history.size());
	if( max_depth > MAX_BOOK_DEPTH ) {
		max_depth = MAX_BOOK_DEPTH;
	}

	worklist wl;

	scoped_lock l(mtx);

	bool all_idle = true;

	timestamp start;
	uint64_t calculated = 0;

	while( true ) {

		while( !stop ) {

			if( wl.empty() && !all_idle ) {
				break;
			}

			while( wl.empty() ) {
				get_work( b, wl, max_depth, max_width, seen, move_history, p );
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
			timestamp now;
			int64_t seconds = (now - start).seconds();
			if( seconds ) {
				std::cerr << std::endl << "Remaining work " << wl.count << " being processed with " << (calculated * 3600) / seconds << " moves/hour" << std::endl;
			}
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

	timestamp start;
	uint64_t calculated = 0;

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
			timestamp now;
			int64_t seconds = (now - start).seconds();
			if( seconds ) {
				std::cerr << std::endl << "Remaining work " << wl.size() << " being processed with " << (calculated * 3600) / seconds << " moves/hour" << std::endl;
			}
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
	std::list<book_entry_with_position> wl = b.get_all_entries();
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

			std::size_t num = 0;
			for( std::size_t i = 0; i < entries.size(); ) {
				++num;
				if( entries[i].is_folded() || (entries[i].search_depth >= MAX_BOOKSEARCH_DEPTH && entries[i].eval_version >= eval_version) || (entries_per_pos != -1 && num > static_cast<std::size_t>(entries_per_pos)) ) {
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

	timestamp start;
	uint64_t calculated = 0;

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
			timestamp now;
			int64_t seconds = (now - start).seconds();
			if( seconds ) {
				std::cerr << "Remaining work " << wl.size() << " being processed with " << (calculated * 3600) / seconds << " moves/hour" << std::endl;
			}
		}

		if( all_idle && stop ) {
			break;
		}
	}

	if( !stop ) {
		std::cerr << "All done" << std::endl;
	}
}


namespace {
std::string side_by_side( std::string const& left, std::string const& right, std::size_t separation = 2 )
{
	std::ostringstream out;

	std::size_t max_width = 0;
	{
		std::istringstream ileft( left );
		std::string line;
		while( std::getline(ileft, line) ) {
			max_width = std::max(max_width, line.size() );
		}
	}

	std::istringstream ileft( left );
	std::istringstream iright( right );

	while( ileft || iright ) {
		std::string lline;
		std::string rline;
		std::getline(ileft, lline);
		std::getline(iright, rline);
		out << std::left << std::setw( max_width + separation ) << lline << rline << std::endl;
	}

	return out.str();
}
}


std::string print_moves( position const& p, std::vector<book_entry> const& moves )
{
	std::string ret = entries_to_string( p, moves );
	if( p.white() ) {
		ret += "White to move\n";
	}
	else {
		ret += "Black to move\n";
	}

	return ret;
}

struct history_entry {
	position p;
	move m;
};

void print_pos( std::vector<history_entry> const& history, position const& p, std::vector<book_entry> const& moves )
{
	std::stringstream ss;
	for( unsigned int i = 0; i < history.size(); ++i ) {
		if( i ) {
			ss << " ";
		}
		if( !(i%2) ) {
			ss << i / 2 + 1<< ". ";
		}
		ss << move_to_san( history[i].p, history[i].m );
	}
	std::string line = ss.str();
	if( !line.empty() ) {
		std::cout << "Line: " << line << std::endl << std::endl;
	}

	std::string mstr = print_moves( p, moves );
	std::string board = board_to_string( p );
	//std::string eval = explain_eval( p, c );

	std::cout << std::endl;
	std::cout << side_by_side( mstr, board );
}


void learnpgn( book& b, std::string const& file )
{
	pgn_reader reader;
	reader.open( file );

	game g;
	while( reader.next( g ) ) {
		std::cerr << ".";
		while( g.moves_.size() > 20 ) {
			g.moves_.pop_back();
		}
		b.mark_for_processing( std::vector<move>(g.moves_.begin(), g.moves_.end()) );
	}
}


bool do_deepen_tree( book& b, position const& p, seen_positions seen, std::vector<move> move_history, int offset )
{
	std::vector<book_entry> entries = b.get_entries( p, move_history );
	if( entries.empty() ) {
		std::stringstream ss;
		ss << "Calculating " << position_to_fen_noclock( p ) << std::endl;
		for( unsigned int i = 0; i < move_history.size(); ++i ) {
			if( i ) {
				ss << " ";
			}
			if( !(i%2) ) {
				ss << i / 2 + 1<< ". ";
			}
			ss << move_to_string( move_history[i], false );
		}
		ss << std::endl;
		std::cerr << ss.str();

		calculate_position( b, p, seen, move_history );
		std::cerr << std::endl;

		return true;
	}

	if( move_history.size() == 20 ) {
		return false;
	}

	book_entry const& first = entries.front();
	for( auto const& e: entries ) {
		if( e.forecast < first.forecast - offset ) {
			break;
		}

		position p2 = p;
		apply_move( p2, e.m );
		move_history.push_back( e.m );
		seen.push_root( get_zobrist_hash( p2 ) );

		int new_offset = std::max( 0, offset - first.forecast + e.forecast );
		bool res = do_deepen_tree( b, p2, seen, move_history, new_offset );
		move_history.pop_back();
		seen.pop_root();
		if( res ) {
			return true;
		}
	}

	return false;
}


void deepen_tree( book& b, position const& p, seen_positions const& seen, std::vector<move> const& move_history, int offset )
{
	bool run = true;
	while( run ) {
		run = false;
		while( do_deepen_tree( b, p, seen, move_history, offset ) ) {
			run = true;
		}
	}
}


void print_stats( book& b )
{
	book_stats stats = b.stats();

	std::stringstream ss;

	ss << "Plies Processed Queued" << std::endl;
	ss << "----------------------" << std::endl;
	for( std::map<uint64_t, stat_entry>::const_iterator it = stats.data.begin(); it != stats.data.end(); ++it ) {
		ss << std::setw( 5 ) << it->first;
		ss << std::setw( 10 ) << it->second.processed;
		ss << std::setw( 7 ) << it->second.queued << std::endl;
	}
	ss << "----------------------" << std::endl;
	ss << std::setw( 5 ) << "Total";
	ss << std::setw( 10 ) << stats.total_processed;
	ss << std::setw( 7 ) << stats.total_queued << std::endl;

	std::cout << ss.str();
}


void run( book& b )
{
	position p;

	std::vector<move> move_history;
	std::vector<history_entry> history;

	seen_positions seen( get_zobrist_hash( p ) );

	{
		std::vector<book_entry> entries = b.get_entries( p, move_history );
		print_pos( history, p, entries );
	}

	unsigned int max_depth = std::min(4u, MAX_BOOK_DEPTH);
	unsigned int max_width = 2;


	while( true ) {
		std::string line;

		std::cout << "Move: ";
		std::getline( std::cin, line );
		if( !std::cin ) {
			break;
		}
		std::string args;
		std::string cmd = split( line, args );
		if( cmd.empty() ) {
			continue;
		}
		else if( cmd == "go" ) {
			go( b, p, seen, move_history, max_depth, max_width );
			return;
		}
		else if( cmd == "process" ) {
			process( b );
		}
		else if( cmd == "size" || cmd == "stats" ) {
			print_stats( b );
		}
		else if( cmd == "book_depth" ) {
			int v = atoi( args.c_str() );
			if( v <= 0 || v > static_cast<int>(MAX_BOOK_DEPTH) ) {
				std::cerr << "Invalid depth: " << v << std::endl;
			}
			else {
				max_depth = v;
				std::cout << "Book depth set to " << v << std::endl;
			}
		}
		else if( cmd == "book_width" ) {
			int v = atoi( args.c_str() );
			if( v <= 0 ) {
				std::cerr << "Invalid width: " << v << std::endl;
			}
			else {
				max_width = v;
				std::cout << "Book width set to " << v << std::endl;
			}
		}
		else if( cmd == "back" ) {
			if( history.empty() ) {
				std::cerr << "Already at top" << std::endl;
			}
			else {
				history_entry h = history.back();
				move_history.pop_back();
				seen.pop_root();
				history.pop_back();
				p = h.p;

				std::vector<book_entry> moves = b.get_entries( p, move_history );
				print_pos( history, p,  moves );
			}
		}
		else if( cmd == "update" ) {
			int v = 5;
			if( !args.empty() ) {
				v = atoi( args.c_str() );
			}
			if( v <= 0 ) {
				v = 5;
			}
			update( b, v );
		}
		else if( cmd == "learnpgn" ) {
			if( args.empty() ) {
				std::cerr << "Need to pass .pgn file as argument" << std::endl;
			}
			else {
				learnpgn( b, args );
			}
		}
		else if( cmd == "deepen" ) {
			move m;
			std::string error;
			if( parse_move( p, args, m, error ) ) {
				deepen_move( b, p, seen, move_history, m );

				std::vector<book_entry> entries = b.get_entries( p, move_history );
				print_pos( history, p, entries );
			}
			else {
				std::cerr << error << std::endl;
			}
		}
		else if( cmd == "fold" ) {
			b.fold();
		}
		else if( cmd == "verify" ) {
			b.fold( true );
		}
		else if( cmd == "insert_log" ) {
			b.set_insert_logfile( args );
		}
		else if( cmd == "redo_hashes" ) {
			b.redo_hashes();
		}
		else if( cmd == "treedeepen" ) {
			int offset = 0;
			if( !args.empty() ) {
				to_int( args, offset, 0, 1000 );
			}
			deepen_tree( b, p, seen, move_history, offset );
		}
		else if( cmd == "historystring" ) {
			std::cerr << b.history_to_string( move_history ) << std::endl;
		}
		else {
			move m;
			std::string error;
			if( parse_move( p, line, m, error ) ) {

				history_entry h;
				h.p = p;
				h.m = m;
				history.push_back( h );

				move_history.push_back( m );

				apply_move( p, m );

				seen.push_root( get_zobrist_hash( p ) );

				std::vector<book_entry> entries = b.get_entries( p, move_history );
				if( entries.empty() ) {
					std::cout << "Position not in book, calculating..." << std::endl;

					if( !calculate_position( b, p, seen, move_history ) ) {
						std::cerr << "Failed to calculate position" << std::endl;
						exit(1);
					}

					entries = b.get_entries( p, move_history );
				}

				if( entries.empty() ) {
					std::cout << "Aborting, cannot add final states to book." << std::endl;
					exit(1);
				}

				print_pos( history, p, entries );
			}
			else {
				std::cerr << error << std::endl;
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

	//signal( SIGINT, &on_signal );

	console_init();

	init_magic();
	init_pst();
	eval_values::init();
	init_random( 1234 );
	init_zobrist_tables();

	transposition_table.init( conf.memory );

	pawn_hash_table.init( conf.pawn_hash_table_size );

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
