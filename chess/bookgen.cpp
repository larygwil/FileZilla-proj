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

bool deepen_move( book& b, position const& p, seen_positions const& seen, std::vector<move> const& move_history, move const& m )
{
	if( !b.is_writable() ) {
		std::cerr << "Cannot deepen move in read-only book." << std::endl;
		return false;
	}

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

	position new_pos = p;
	apply_move( new_pos, m );

	short value;
	if( seen.is_two_fold( new_pos.hash_, 1 ) ) {
		value = 0;
	}
	else {
		seen_positions seen2 = seen;
		seen2.push_root( new_pos.hash_ );

		calc_manager cmgr;
		calc_result res = cmgr.calc( new_pos, MAX_BOOKSEARCH_DEPTH - 2, timestamp(), duration::infinity(),
				duration::infinity(), move_history.size() % 256, seen2, null_new_best_move_cb );

		value = -res.forecast;
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
	calculate_moves<movegen_type::all>( p, pm, check );
	if( pm == moves ) {
		return true;
	}

	std::vector<book_entry> entries;

	calc_manager cmgr;
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
				duration::infinity(), move_history.size() % 256, seen2, null_new_best_move_cb );

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
					duration::infinity(), move_history.size() % 256, seen2, null_new_best_move_cb );

				value = -res.forecast;
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
	calc_manager cmgr;

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
					duration::infinity(), move_history.size() % 256, seen2, null_new_best_move_cb );

				value = -res.forecast;
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

	seen_positions seen( p.hash_ );

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


void go( book& b, position const& p, seen_positions const& seen, std::vector<move> const& move_history, unsigned int max_depth, unsigned int max_width )
{
	max_depth += static_cast<unsigned int>(move_history.size());
	if( max_depth > MAX_BOOK_DEPTH ) {
		max_depth = MAX_BOOK_DEPTH;
	}

	worklist wl;

	timestamp start;
	uint64_t calculated = 0;

	while( true ) {

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
		}
		if( wl.empty() ) {
			break;
		}

		std::cerr << std::endl << "Created worklist with " << wl.count << " positions to evaluate" << std::endl;

		work w;
		while( get_next( wl, w ) ) {
			calculate_position( b, w.p, w.seen, w.move_history );

			++calculated;
			timestamp now;
			int64_t seconds = (now - start).seconds();
			if( seconds ) {
				std::cerr << std::endl << "Remaining work " << wl.count << " being processed with " << (calculated * 3600) / seconds << " moves/hour.";
				uint64_t eta = seconds / calculated * wl.count;
				std::cerr << " Estimated completion in " << eta / 60 << " minutes" << std::endl; 
			}
		}
	}
}


void process( book& b )
{
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

		calculate_position( b, w.p, w.seen, w.move_history );

		++calculated;
		timestamp now;
		int64_t seconds = (now - start).seconds();
		if( seconds ) {
			std::cerr << std::endl << "Remaining work " << wl.size() << " being processed with " << (calculated * 3600) / seconds << " moves/hour.";
			uint64_t eta = seconds / calculated * wl.size();
			std::cerr << " Estimated completion in " << eta / 60 << " minutes" << std::endl; 
		}
	}
}


void update( book& b, int entries_per_pos = 5 )
{
	if( !b.is_writable() ) {
		std::cerr << "Book is read-only" << std::endl;
		return;
	}

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
		update_position( b, w.w.p, w.w.seen, w.w.move_history, w.entries );

		++calculated;
		timestamp now;
		int64_t seconds = (now - start).seconds();
		if( seconds ) {
			std::cerr << "Remaining work " << wl.size() << " being processed with " << (calculated * 3600) / seconds << " moves/hour.";
			uint64_t eta = seconds / calculated * wl.size();
			std::cerr << " Estimated completion in " << eta / 60 << " minutes" << std::endl; 
		}
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


bool learnpgn( book& b, std::string const& file )
{
	pgn_reader reader;
	if( !reader.open( file ) ) {
		return false;
	}

	game g;
	while( reader.next( g ) ) {
		std::cerr << ".";
		while( g.moves_.size() > 20 ) {
			g.moves_.pop_back();
		}
		b.mark_for_processing( std::vector<move>(g.moves_.begin(), g.moves_.end()) );
	}

	return true;
}


bool do_deepen_tree( book& b, position const& p, seen_positions seen, std::vector<move> move_history, int offset )
{
	std::vector<book_entry> entries = b.get_entries( p, move_history );
	if( entries.empty() ) {
		std::stringstream ss;
		ss << "Calculating " << position_to_fen_noclock( p ) << std::endl;

		position p2;
		for( unsigned int i = 0; i < move_history.size(); ++i ) {
			if( i ) {
				ss << " ";
			}
			if( !(i%2) ) {
				ss << i / 2 + 1<< ". ";
			}
			ss << move_to_san( p2, move_history[i] );
			apply_move( p2, move_history[i] );
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
		seen.push_root( p2.hash_ );

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


bool deepen_tree( book& b, position const& p, seen_positions const& seen, std::vector<move> const& move_history, int offset )
{
	if( !b.is_writable() ) {
		std::cerr << "Cannot deepen tree if book is read-only." << std::endl;
		return false;
	}
	bool run = true;
	while( run ) {
		run = false;
		while( do_deepen_tree( b, p, seen, move_history, offset ) ) {
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


bool parse_move_bg( book& b, std::vector<move> const& move_history, position const& p, std::string const& line, move& m, std::string& error )
{
	std::size_t i;

	std::vector<book_entry> moves = b.get_entries( p, move_history );
	if( to_int<std::size_t>( line, i, 1, moves.size() ) ) {
		m = moves[i-1].m;
		return true;
	}

	return parse_move( p, line, m, error );
}


void run( book& b )
{
	position p;

	color::type view = color::white;
	std::vector<move> move_history;
	
	seen_positions seen( p.hash_ );

	{
		std::vector<book_entry> entries = b.get_entries( p, move_history );
		print_pos( move_history, entries, view );
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
		else if( cmd == "quit" ) {
			 break;
		}
		else if( cmd == "exit") {
			break;
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
			if( move_history.empty() ) {
				std::cerr << "Already at top" << std::endl;
			}
			else {
				int count = 1;
				if( !args.empty() ) {
					to_int( args, count, 1, static_cast<int>(move_history.size()) );
				}
				for( int i = 0; i < count; ++i ) {
					move_history.pop_back();
					seen.pop_root();
					p.reset();
					for( auto m : move_history ) {
						apply_move( p, m );
					}
				}

				std::vector<book_entry> moves = b.get_entries( p, move_history );
				print_pos( move_history, moves, view );
			}
		}
		else if( cmd == "new" || cmd == "reset" ) {
			view = color::white;
			move_history.clear();
			p.reset();
			seen.reset_root( p.hash_ );

			std::vector<book_entry> moves = b.get_entries( p, move_history );
			print_pos( move_history, moves, view );
		}
		else if( cmd == "flip" ) {
			view = static_cast<color::type>(1 - view);

			std::vector<book_entry> moves = b.get_entries( p, move_history );
			print_pos( move_history, moves, view );
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
				if( !learnpgn( b, args ) ) {
					std::cerr << "Failed to learn from .pgn" << std::endl;
				}
			}
		}
		else if( cmd == "deepen" ) {
			move m;
			std::string error;
			if( parse_move_bg( b, move_history, p, args, m, error ) ) {
				if( deepen_move( b, p, seen, move_history, m ) ) {
					std::vector<book_entry> entries = b.get_entries( p, move_history );
					print_pos( move_history, entries, view );
				}
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
		else if( cmd == "export" ) {
			b.export_book( args );
		}
		else {
			move m;
			std::string error;
			if( parse_move_bg( b, move_history, p, line, m, error ) ) {

				move_history.push_back( m );

				apply_move( p, m );
				seen.push_root( p.hash_ );

				std::vector<book_entry> entries = b.get_entries( p, move_history );
				if( entries.empty() ) {
					if( b.is_writable() ) {
						std::cout << "Position not in book, calculating..." << std::endl;

						if( !calculate_position( b, p, seen, move_history ) ) {
							std::cerr << "Failed to calculate position" << std::endl;
							exit(1);
						}
					}
					else {
						std::cerr << "Position not in book and book is read-only." << std::endl;
						move_history.pop_back();
						seen.pop_root();
						
						p.reset();
						for( auto m : move_history ) {
							apply_move( p, m );
						}
					}

					entries = b.get_entries( p, move_history );
				}

				if( entries.empty() ) {
					std::cout << "Aborting, cannot add final states to book." << std::endl;
					exit(1);
				}

				print_pos( move_history, entries, view );
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

	console_init();
	logger::show_debug( false );

	init_magic();
	init_pst();
	eval_values::init();
	init_random( 1234 );
	init_zobrist_tables();

	transposition_table.init( conf.memory );

	pawn_hash_table.init( conf.pawn_hash_table_size() );

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
