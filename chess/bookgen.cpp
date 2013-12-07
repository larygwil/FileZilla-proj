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
#include "util/logger.hpp"
#include "util/string.hpp"
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

int const MAX_BOOKSEARCH_DEPTH = 20;

unsigned int const MAX_BOOK_DEPTH = 10;

void print_remaining( timestamp const& start, uint64_t total, uint64_t calculated, std::string const& name = "moves" )
{
	std::cerr << std::endl << (total - calculated) << " " << name << " remaining.";

	timestamp now;
	int64_t seconds = (now - start).seconds();
	if( seconds ) {
		std::cerr << " Processing " << (calculated * 3600) / seconds << " " << name << "/hour.";
		uint64_t eta = seconds / calculated * total;
		std::cerr << " Estimated completion in " << eta / 60 << " minutes";
	}
	std::cerr << std::endl;
}

bool deepen_move( context& ctx, book& b, position const& p, seen_positions const& seen, std::vector<move> const& history, move const& m )
{
	ctx.tt_.init( ctx.conf_.memory );

	if( !b.is_writable() ) {
		std::cerr << "Cannot deepen move in read-only book." << std::endl;
		return false;
	}

	int depth = MAX_BOOKSEARCH_DEPTH;

	{
		std::vector<book_entry> entries = b.get_entries( p, history );
		for( std::vector<book_entry>::const_iterator it = entries.begin(); it != entries.end(); ++it ) {
			if( it->m != m ) {
				continue;
			}
			if( it->search_depth >= MAX_BOOKSEARCH_DEPTH ) {
				depth = it->search_depth + 1;
			}
		}
	}

	position new_pos = p;
	apply_move( new_pos, m );

	short value;
	if( seen.is_two_fold( new_pos.hash_, 1 ) ) {
		value = 0;
	}
	else {
		seen_positions seen2 = seen;
		seen2.push_root( new_pos.hash_ );

		calc_manager cmgr( ctx );
		calc_result res = cmgr.calc( new_pos, MAX_BOOKSEARCH_DEPTH - 2, timestamp(), duration::infinity(),
				duration::infinity(), history.size() % 256, seen2, null_new_best_move_cb );

		value = -res.forecast;
	}

	book_entry e;
	e.forecast = value;
	e.m = m;
	e.search_depth = depth;
	b.update_entry( history, e );

	return true;
}


bool calculate_position( context& ctx, book& b, position const& p, seen_positions const& seen, std::vector<move> const& history )
{
	ctx.tt_.init( ctx.conf_.memory );

	move_info moves[200];
	move_info* pm = moves;
	check_map check( p );
	calculate_moves<movegen_type::all>( p, pm, check );
	if( pm == moves ) {
		return true;
	}

	std::vector<book_entry> entries;

	calc_manager cmgr( ctx );
	for( move_info const* it = moves; it != pm; ++it ) {

		position new_pos = p;
		apply_move( new_pos, it->m );

		short value;
		if( seen.is_two_fold( new_pos.hash_, 1 ) ) {
			value = 0;
		}
		else {
			seen_positions seen2 = seen;
			seen2.push_root( new_pos.hash_ );

			calc_result res = cmgr.calc( new_pos, MAX_BOOKSEARCH_DEPTH - 2, timestamp(), duration::infinity(),
				duration::infinity(), history.size() % 256, seen2, null_new_best_move_cb );

			value = -res.forecast;
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

			position new_pos = p;
			apply_move( new_pos, entry.m );

			short value;
			if( seen.is_two_fold( new_pos.hash_, 1 ) ) {
				value = 0;
			}
			else {
				seen_positions seen2 = seen;
				seen2.push_root( new_pos.hash_ );

				calc_result res = cmgr.calc( new_pos, MAX_BOOKSEARCH_DEPTH, timestamp(), duration::infinity(),
					duration::infinity(), history.size() % 256, seen2, null_new_best_move_cb );

				value = -res.forecast;
			}

			entry.search_depth = MAX_BOOKSEARCH_DEPTH;
			entry.forecast = value;

			std::cerr << "F";
		}
	}

	return b.add_entries( history, entries );
}


bool update_position( context& ctx, book& b, position const& p, seen_positions const& seen, std::vector<move> const& history, std::vector<book_entry> const& entries )
{
	calc_manager cmgr( ctx );

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

			position new_pos = p;
			apply_move( new_pos, entry.m );

			short value;
			if( seen.is_two_fold( new_pos.hash_, 1 ) ) {
				value = 0;
			}
			else {
				seen_positions seen2 = seen;
				seen2.push_root( new_pos.hash_ );

				calc_result res = cmgr.calc( new_pos, new_depth, timestamp(), duration::infinity(),
					duration::infinity(), history.size() % 256, seen2, null_new_best_move_cb );

				value = -res.forecast;
			}

			std::cerr << entry.forecast << " d" << static_cast<int>(entry.search_depth) << " v" << static_cast<int>(entry.eval_version) << " -> " << value << " d" << new_depth << " " << history.size() << " " << position_to_fen_noclock( ctx.conf_, p ) << " " << move_to_san( p, entry.m ) << std::endl;

			entry.forecast = value;
			entry.search_depth = new_depth;

			b.update_entry( history, entry );
		}
	}

	return true;
}


void init_book( context& ctx, book& b )
{
	position p;

	seen_positions seen( p.hash_ );

	std::vector<move> history;

	if( !calculate_position( ctx, b, p, seen, history ) ) {
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


void get_work( book& b, worklist& wl, int max_depth, unsigned int max_width, seen_positions const& seen, std::vector<move> const& history, position const& p )
{
	std::vector<book_entry> moves = b.get_entries( p, history );

	unsigned int i = 0;
	for( std::vector<book_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it, ++i ) {
		if( i >= max_width ) {
			break;
		}

		position new_pos = p;
		apply_move( new_pos, it->m );

		std::vector<move> child_history = history;
		child_history.push_back( it->m );
		std::vector<book_entry> child_moves = b.get_entries( new_pos, child_history );

		seen_positions child_seen = seen;
		child_seen.push_root( new_pos.hash_ );

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


void go( context& ctx, book& b, position const& p, seen_positions const& seen, std::vector<move> const& history, unsigned int max_depth, unsigned int max_width )
{
	ctx.tt_.init( ctx.conf_.memory );

	max_depth += static_cast<unsigned int>(history.size());
	if( max_depth > MAX_BOOK_DEPTH ) {
		max_depth = MAX_BOOK_DEPTH;
	}

	worklist wl;

	timestamp start;
	uint64_t calculated = 0;

	while( true ) {

		while( wl.empty() ) {
			get_work( b, wl, max_depth, max_width, seen, history, p );
			if( wl.empty() ) {
				if( max_depth < MAX_BOOK_DEPTH ) {
					++max_depth;
				}
				else {
					break;
				}
			}
		}
		if( wl.empty() ) {
			break;
		}

		std::cerr << std::endl << "Created worklist with " << wl.count << " positions to evaluate" << std::endl;

		work w;
		while( get_next( wl, w ) ) {
			calculate_position( ctx, b, w.p, w.seen, w.move_history );

			++calculated;
			print_remaining( start, wl.count, calculated );
		}
	}
}


void process( context& ctx, book& b )
{
	ctx.tt_.init( ctx.conf_.memory );

	std::list<work> wl = b.get_unprocessed_positions();
	if( wl.empty() ) {
		return;
	}

	std::cerr << "Got " << wl.size() << " positions to calculate" << std::endl;

	timestamp start;
	uint64_t calculated = 0;

	while( !wl.empty() ) {

		work w = wl.front();
		wl.pop_front();

		calculate_position( ctx, b, w.p, w.seen, w.move_history );

		++calculated;
		print_remaining( start, wl.size(), calculated );
	}
}


void update( context& ctx, book& b, int entries_per_pos = 5 )
{
	ctx.tt_.init( ctx.conf_.memory );

	if( !b.is_writable() ) {
		std::cerr << "Book is read-only" << std::endl;
		return;
	}

	std::cerr << "Collecting all positions with at most " << entries_per_pos << " moves to update per position.\n";

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

	timestamp start;
	uint64_t calculated = 0;

	while( !wl.empty() ) {

		book_entry_with_position w = wl.front();
		wl.pop_front();
		update_position( ctx, b, w.w.p, w.w.seen, w.w.move_history, w.entries );

		++calculated;
		print_remaining( start, wl.size(), calculated );
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


void print_pos( std::vector<move> const& history, std::vector<book_entry> const& moves, color::type view )
{
	position p;

	std::stringstream ss;
	for( unsigned int i = 0; i < history.size(); ++i ) {
		if( i ) {
			ss << " ";
		}
		if( !(i%2) ) {
			ss << i / 2 + 1<< ". ";
		}
		ss << move_to_san( p, history[i] );
		apply_move( p, history[i] );
	}
	std::string line = ss.str();
	if( !line.empty() ) {
		std::cout << "Line: " << line << std::endl << std::endl;
	}

	std::string mstr = print_moves( p, moves );
	std::string board = board_to_string( p, view );

	std::cout << std::endl;
	std::cout << side_by_side( mstr, board );
	std::cout.flush();
}


bool learnpgn( context& ctx, book& b, std::string const& file, bool defer )
{
	timestamp start;

	pgn_reader reader;
	if( !reader.open( file ) ) {
		return false;
	}

	unsigned int count = reader.size();
	unsigned int calculated = 0;

	std::cout << "Got " << count << " games to analyze." << std::endl;

	game g;
	while( reader.next( g ) ) {
		while( g.moves_.size() > 20 ) {
			g.moves_.pop_back();
		}
		if( defer ) {
			b.mark_for_processing( std::vector<move>(g.moves_.begin(), g.moves_.end()) );
			std::cerr << ".";
		}
		else {
			position p;
			std::vector<move> h;
			seen_positions seen( p.hash_ );

			for( auto m : g.moves_ ) {
				apply_move( p, m );
				seen.push_root( p.hash_ );
				h.push_back( m );

				std::vector<book_entry> entries = b.get_entries( p, h );
				if( entries.empty() ) {
					calculate_position( ctx, b, p, seen, h );
				}
			}
		}

		++calculated;
		if( !defer ) {
			print_remaining( start, count, calculated, "games" );
		}
	}

	return true;
}


bool do_deepen_tree( context& ctx, book& b, position const& p, seen_positions seen, std::vector<move> history, int offset )
{
	std::vector<book_entry> entries = b.get_entries( p, history );
	if( entries.empty() ) {
		std::stringstream ss;
		ss << "Calculating " << position_to_fen_noclock( ctx.conf_, p ) << std::endl;

		position p2;
		for( unsigned int i = 0; i < history.size(); ++i ) {
			if( i ) {
				ss << " ";
			}
			if( !(i%2) ) {
				ss << i / 2 + 1<< ". ";
			}
			ss << move_to_san( p2, history[i] );
			apply_move( p2, history[i] );
		}
		ss << std::endl;
		std::cerr << ss.str();

		calculate_position( ctx, b, p, seen, history );
		std::cerr << std::endl;

		return true;
	}

	if( history.size() == 20 ) {
		return false;
	}

	book_entry const& first = entries.front();
	for( auto const& e: entries ) {
		if( e.forecast < first.forecast - offset ) {
			break;
		}

		position p2 = p;
		apply_move( p2, e.m );
		history.push_back( e.m );
		seen.push_root( p2.hash_ );

		int new_offset = std::max( 0, offset - first.forecast + e.forecast );
		bool res = do_deepen_tree( ctx, b, p2, seen, history, new_offset );
		history.pop_back();
		seen.pop_root();
		if( res ) {
			return true;
		}
	}

	return false;
}


bool deepen_tree( context& ctx, book& b, position const& p, seen_positions const& seen, std::vector<move> const& history, int offset )
{
	ctx.tt_.init( ctx.conf_.memory );

	if( !b.is_writable() ) {
		std::cerr << "Cannot deepen tree if book is read-only." << std::endl;
		return false;
	}
	bool run = true;
	while( run ) {
		run = false;
		while( do_deepen_tree( ctx, b, p, seen, history, offset ) ) {
			run = true;
		}
	}

	return true;
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


bool parse_move_bg( book& b, std::vector<move> const& history, position const& p, std::string const& line, move& m, std::string& error )
{
	std::size_t i;

	std::vector<book_entry> moves = b.get_entries( p, history );
	if( to_int<std::size_t>( line, i, 1, moves.size(), false ) ) {
		m = moves[i-1].m;
		return true;
	}

	return parse_move( p, line, m, error );
}


struct bookgen_state
{
	bookgen_state()
		: seen( p.hash_ )
		, view( color::white )
		, max_depth( std::min(4u, MAX_BOOK_DEPTH) )
		, max_width( 2 )
	{
	}

	position p;
	std::vector<move> history;
	seen_positions seen;
	color::type view;

	unsigned int max_depth;
	unsigned int max_width;
};


bool process_command( context& ctx, book& b, std::string const& cmd, std::string const& args, bookgen_state& state )
{
	if( cmd.empty() ) {
		return true;
	}
	else if( cmd == "quit" || cmd == "exit" ) {
		exit( 0 );
	}
	else if( cmd == "go" ) {
		go( ctx, b, state.p, state.seen, state.history, state.max_depth, state.max_width );
		std::vector<book_entry> moves = b.get_entries( state.p, state.history );
		print_pos( state.history, moves, state.view );
	}
	else if( cmd == "process" ) {
		process( ctx, b );
	}
	else if( cmd == "size" || cmd == "stats" ) {
		print_stats( b );
	}
	else if( cmd == "book_depth" ) {
		int v = atoi( args.c_str() );
		if( v <= 0 || v > static_cast<int>(MAX_BOOK_DEPTH) ) {
			std::cerr << "Invalid depth: " << v << std::endl;
			return false;
		}
		else {
			state.max_depth = v;
			std::cout << "Book depth set to " << v << std::endl;
		}
	}
	else if( cmd == "book_width" ) {
		int v = atoi( args.c_str() );
		if( v <= 0 ) {
			std::cerr << "Invalid width: " << v << std::endl;
			return false;
		}
		else {
			state.max_width = v;
			std::cout << "Book width set to " << v << std::endl;
		}
	}
	else if( cmd == "back" ) {
		if( state.history.empty() ) {
			std::cerr << "Already at top" << std::endl;
			return false;
		}
		else {
			int count = 1;
			if( !args.empty() ) {
				if( !to_int( args, count, 1, static_cast<int>(state.history.size()) ) ) {
					return false;
				}
			}
			for( int i = 0; i < count; ++i ) {
				state.history.pop_back();
				state.seen.pop_root();
				state.p.reset();
				for( auto m : state.history ) {
					apply_move( state.p, m );
				}
			}

			std::vector<book_entry> moves = b.get_entries( state.p, state.history );
			print_pos( state.history, moves, state.view );
		}
	}
	else if( cmd == "new" || cmd == "reset" ) {
		state.view = color::white;
		state.history.clear();
		state.p.reset();
		state.seen.reset_root( state.p.hash_ );

		std::vector<book_entry> moves = b.get_entries( state.p, state.history );
		print_pos( state.history, moves, state.view );
	}
	else if( cmd == "flip" ) {
		state.view = other(state.view);

		std::vector<book_entry> moves = b.get_entries( state.p, state.history );
		print_pos( state.history, moves, state.view );
	}
	else if( cmd == "update" ) {
		int v = 5;
		if( !args.empty() ) {
			v = atoi( args.c_str() );
		}
		if( v <= 0 ) {
			v = 5;
		}
		update( ctx, b, v );
	}
	else if( cmd == "learnpgn" ) {
		auto tokens = tokenize( args );
		std::string file;
		if( !tokens.empty() ) {
			file = tokens.back();
			tokens.pop_back();
		}

		bool defer = false;
		for( auto tok : tokens ) {
			if( tok == "--defer" ) {
				defer = true;
			}
			else {
				std::cerr << "Invalid option: " << tok;
			}

		}

		if( file.empty() ) {
			std::cerr << "Need to pass .pgn file as argument" << std::endl;
			return false;
		}
		else {
			if( !learnpgn( ctx, b, file, defer ) ) {
				std::cerr << "Failed to learn from .pgn" << std::endl;
				return false;
			}
		}
	}
	else if( cmd == "deepen" ) {
		move m;
		std::string error;
		if( parse_move_bg( b, state.history, state.p, args, m, error ) ) {
			if( deepen_move( ctx, b, state.p, state.seen, state.history, m ) ) {
				std::vector<book_entry> entries = b.get_entries( state.p, state.history );
				print_pos( state.history, entries, state.view );
			}
		}
		else {
			std::cerr << error << std::endl;
			return false;
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
			if( !to_int( args, offset, 0, 1000 ) ) {
				return false;
			}
		}
		deepen_tree( ctx, b, state.p, state.seen, state.history, offset );
	}
	else if( cmd == "export" ) {
		b.export_book( args );
	}
	else {
		move m;
		std::string error;
		if( parse_move_bg( b, state.history, state.p, cmd, m, error ) ) {

			state.history.push_back( m );

			apply_move( state.p, m );
			state.seen.push_root( state.p.hash_ );

			std::vector<book_entry> entries = b.get_entries( state.p, state.history );
			if( entries.empty() ) {
				if( b.is_writable() ) {
					std::cout << "Position not in book, calculating..." << std::endl;

					if( !calculate_position( ctx, b, state.p, state.seen, state.history ) ) {
						std::cerr << "Failed to calculate position" << std::endl;
						exit(1);
					}
				}
				else {
					std::cerr << "Position not in book and book is read-only." << std::endl;
					state.history.pop_back();
					state.seen.pop_root();

					state.p.reset();
					for( auto m : state.history ) {
						apply_move( state.p, m );
					}
				}

				entries = b.get_entries( state.p, state.history );
			}

			if( entries.empty() ) {
				std::cout << "Aborting, cannot add final states to book." << std::endl;
				exit(1);
			}

			print_pos( state.history, entries, state.view );
		}
		else {
			std::cerr << error << std::endl;
			return false;
		}
	}

	return true;
}


void run( context& ctx, book& b )
{
	bookgen_state state;

	{
		std::vector<book_entry> entries = b.get_entries( state.p, state.history );
		print_pos( state.history, entries, state.view );
	}

	while( true ) {
		std::string line;

		std::cout << "Move: ";
		std::getline( std::cin, line );
		if( !std::cin ) {
			break;
		}
		std::string args;
		std::string cmd = split( line, args );

		process_command( ctx, b, cmd, args, state );
	}
}

int main( int argc, char const* argv[] )
{
	std::cout << "Initializing" << std::endl;

	context ctx;
	std::string command = ctx.conf_.init( argc, argv );

	std::string book_dir;
	std::string self = argv[0];
	if( self.rfind('/') != std::string::npos ) {
		book_dir = self.substr( 0, self.rfind('/') + 1 );
	}

	console_init();
	logger::show_debug( false );

	init_magic();
	pst.init();
	eval_values::init();
	init_zobrist_tables();

	ctx.pawn_tt_.init( ctx.conf_.pawn_hash_table_size() );

	std::cout << "Opening book" << std::endl;

	book b( book_dir );
	if( !b.is_open() ) {
		std::cerr << "Cound not open opening book" << std::endl;
		return 1;
	}

	if( !b.size() ) {
		init_book( ctx, b );
	}

	if( command.empty() ) {
		std::cout << "Ready" << std::endl;
		run( ctx, b );
	}
	else {
		std::string args;
		command = split( command, args );

		bookgen_state s;
		if( !process_command( ctx, b, command, args, s ) ) {
			return 1;
		}
	}

	return 0;
}
