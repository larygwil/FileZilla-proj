/*
Octochess
---------

Copyright (C) 2011 Tim "codesquid" Kosse
http://filezilla-project.org/

Distributed under the terms and conditions of the GNU General Public License v3.

If you want to purchase a copy of Octochess under a different license, please
contact tim.kosse@filezilla-project.org for details.


*/

#include "book.hpp"
#include "chess.hpp"
#include "config.hpp"
#include "calc.hpp"
#include "eval.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "logger.hpp"
#include "mobility.hpp"
#include "moves.hpp"
#include "util.hpp"
#include "pawn_structure_hash_table.hpp"
#include "platform.hpp"
#include "statistics.hpp"
#include "selftest.hpp"
#include "tweak.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <list>
#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <sstream>

const int TIME_LIMIT = 90000; //30000;

const int PAWN_HASH_TABLE_SIZE = 10;

std::string book_dir;

void auto_play()
{
	if( conf.depth == -1 ) {
		conf.depth = 8;
	}
	transposition_table.init( conf.memory );
	pawn_hash_table.init( PAWN_HASH_TABLE_SIZE );
	unsigned long long start = get_time();
	position p;

	init_board(p);

	unsigned int i = 1;
	color::type c = color::white;
	move m;
	int res = 0;

	seen_positions seen;
	seen.root_position = 0;
	seen.pos[0] = get_zobrist_hash( p );

	short last_mate = 0;

	while( calc( p, c, m, res, TIME_LIMIT * timer_precision() / 1000, TIME_LIMIT * timer_precision() / 1000, i, seen, last_mate ) ) {
		if( c == color::white ) {
			std::cout << std::setw(3) << i << ".";
		}

		if( res > result::win_threshold ) {
			last_mate = res;
		}
		std::cout << " " << move_to_string(p, c, m) << std::endl;

		if( c == color::black ) {
			++i;
			std::cout << std::endl;
			if( conf.max_moves && i > conf.max_moves ) {
				break;
			}
		}

		if( !validate_move( p, m, c ) ) {
			std::cerr << std::endl << "NOT A VALID MOVE" << std::endl;
			exit(1);
		}

		bool reset_seen = false;
		if( m.piece == pieces::pawn || m.captured_piece ) {
			reset_seen = true;
		}

		apply_move( p, m, c );
		int ev = evaluate_fast( p, color::white );
		std::cerr << "Evaluation (for white): " << ev << " centipawns" << std::endl;

		//std::cerr << explain_eval( p, color::white, p.bitboards );

		c = static_cast<color::type>(1-c);

		if( !reset_seen ) {
			seen.pos[++seen.root_position] = get_zobrist_hash( p );
		}
		else {
			seen.root_position = 0;
			seen.pos[0] = get_zobrist_hash( p );
		}

		if( seen.root_position > 110 ) { // Be lenient, 55 move rule is fine for us in case we don't implement this correctly.
			std::cerr << "DRAW" << std::endl;
			exit(1);
		}
	}

	unsigned long long stop = get_time();

	std::cerr << std::endl << "Runtime: " << (stop - start) * 1000 / timer_precision() << " ms " << std::endl;

#ifdef USE_STATISTICS
	print_stats( start, stop );
#endif
}

struct xboard_state
{
	xboard_state()
		: c()
		, clock()
		, book_( book_dir )
		, in_book( book_.is_open() )
		, time_remaining()
		, bonus_time()
		, force(true)
		, self(color::black)
		, hash_initialized()
		, time_control()
		, time_increment()
		, history()
		, post(true)
		, last_mate()
		, started_from_root()
		, internal_overhead( 100 * timer_precision() / 1000 )
		, communication_overhead( 50 * timer_precision() / 1000 )
		, last_go_time()
		, last_go_color()
		, moves_between_updates()
		, level_cmd_differences()
		, level_cmd_count()
	{
		reset();
	}

	void reset()
	{
		init_board(p);
		in_book = book_.is_open();
		if( in_book ) {
			std::cerr << "Opening book loaded" << std::endl;
		}
		move_history_.clear();

		c = color::white;

		clock = 1;

		seen = seen_positions();
		seen.root_position = 0;
		seen.pos[0] = get_zobrist_hash( p );

		time_remaining = conf.time_limit * timer_precision() / 1000;
		bonus_time = 0;

		force = false;
		self = color::black;

		last_mate = 0;

		started_from_root = true;
	}

	void apply( move const& m )
	{
		history.push_back( p );
		bool reset_seen = false;
		if( m.piece == pieces::pawn || m.captured_piece ) {
			reset_seen = true;
		}

		apply_move( p, m, c );
		++clock;
		c = static_cast<color::type>( 1 - c );

		if( !reset_seen ) {
			seen.pos[++seen.root_position] = get_zobrist_hash( p );
		}
		else {
			seen.root_position = 0;
			seen.pos[0] = get_zobrist_hash( p );
		}

		if( seen.root_position >= 110 ) {
			std::cout << "1/2-1/2 (Draw)" << std::endl;
		}

		move_history_.push_back( m );
	}

	bool undo( unsigned int count )
	{
		if( !count || count > history.size() ) {
			return false;
		}

		clock -= count;
		seen.root_position -= count;

		if( count % 2 ) {
			c = static_cast<color::type>(1 - c);
		}

		while( --count ) {
			history.pop_back();
			move_history_.pop_back();
		}
		p = history.back();
		history.pop_back();
		move_history_.pop_back();

		bonus_time = false;
		in_book = false;

		return true;
	}

	void update_comm_overhead( unsigned long long new_remaining )
	{
		if( c == last_go_color && moves_between_updates && std::abs(static_cast<signed long long>(time_remaining) - static_cast<signed long long>(new_remaining)) < 2 * timer_precision() * moves_between_updates ) {
			level_cmd_differences += (static_cast<signed long long>(time_remaining) - static_cast<signed long long>(new_remaining));
			level_cmd_count += moves_between_updates;

			unsigned long long comm_overhead;
			if( level_cmd_differences >= 0 ) {
				comm_overhead = level_cmd_differences / level_cmd_count;
			}
			else {
				comm_overhead = 0;
			}

			std::cerr << "Updating communication overhead from " << communication_overhead * 1000 / timer_precision() << " ms to " << comm_overhead * 1000 / timer_precision() << " ms " << std::endl;
			communication_overhead = comm_overhead;
		}

		moves_between_updates = 0;
	}

	position p;
	color::type c;
	int clock;
	seen_positions seen;
	book book_;
	bool in_book;
	unsigned long long time_remaining;
	unsigned long long bonus_time;
	bool force;
	color::type self;
	bool hash_initialized;
	unsigned long long time_control;
	unsigned long long time_increment;

	std::list<position> history;
	std::vector<move> move_history_;

	bool post;

	short last_mate;

	bool started_from_root;

	// If we calculate move time of x but consume y > x amount of time, internal overhead if y - x.
	// This is measured locally between receiving the go and sending out the reply.
	unsigned long long internal_overhead;

	// If we receive time updates between moves, communication_overhead is the >=0 difference between two timer updates
	// and the calculated time consumption.
	unsigned long long communication_overhead;
	unsigned long long last_go_time;

	color::type last_go_color;
	unsigned int moves_between_updates;

	// Level command is in seconds only. Hence we need to accumulate data before we can update the
	// communication overhead.
	signed long long level_cmd_differences;
	unsigned long long level_cmd_count;
};


volatile extern bool do_abort;

class xboard_thread : public thread, public new_best_move_callback
{
public:
	xboard_thread( xboard_state& s );
	~xboard_thread();

	virtual void onRun();

	void start();

	move stop();

	mutex mtx;

	virtual void on_new_best_move( position const& p, color::type c, int depth, int evaluation, unsigned long long nodes, pv_entry const* pv );

private:
	bool abort;
	xboard_state& state;
	move best_move;
};


xboard_thread::xboard_thread( xboard_state& s )
: abort()
, state(s)
{
}


xboard_thread::~xboard_thread()
{
	stop();
}


void xboard_thread::onRun()
{
	if( state.bonus_time > state.time_remaining ) {
		state.bonus_time = 0;
	}

	unsigned long long remaining_moves;
	if( !state.time_control ) {
		remaining_moves = (std::max)( 15, (80 - state.clock) / 2 );
	}
	else {
		remaining_moves = (state.time_control * 2) - (state.clock % (state.time_control * 2));
	}
	unsigned long long time_limit = (state.time_remaining - state.bonus_time) / remaining_moves + state.bonus_time;
	unsigned long long overhead = state.internal_overhead + state.communication_overhead;

	if( state.time_increment && state.time_remaining > (time_limit + state.time_increment) ) {
		time_limit += state.time_increment;
	}

	if( time_limit > overhead ) {
		time_limit -= overhead;
	}
	else {
		// Any less time makes no sense.
		time_limit = 10 * 1000 / timer_precision();
	}

	// Any less time makes no sense.
	if( time_limit < 10 * 1000 / timer_precision() ) {
		time_limit = 10 * 1000 / timer_precision();
	}

	move m;
	int res;
	bool success = calc( state.p, state.c, m, res, time_limit, state.time_remaining, state.clock, state.seen, state.last_mate, *this );

	scoped_lock l( mtx );

	if( abort ) {
		return;
	}

	bool ponder = false;
	if( success ) {

		std::cout << "move " << move_to_string( state.p, state.c, m ) << std::endl;

		state.apply( m );

		{
			int i = evaluate_fast( state.p, static_cast<color::type>(1-state.c) );
			std::cerr << "  ; Current evaluation: " << i << " centipawns, forecast " << res << std::endl;
			
			//std::cerr << explain_eval( state.p, static_cast<color::type>(1-state.c), p.bitboards );
		}

		if( res > result::win_threshold ) {
			state.last_mate = res;
		}
		else {
			ponder = conf.ponder;
		}
	}
	else {
		if( res == result::win ) {
			std::cout << "1-0 (White wins)" << std::endl;
		}
		else if( res == result::loss ) {
			std::cout << "0-1 (Black wins)" << std::endl;
		}
		else {
			std::cout << "1/2-1/2 (Draw)" << std::endl;
		}
	}
	unsigned long long stop = get_time();
	unsigned long long elapsed = stop - state.last_go_time;

	std::cerr << "Elapsed: " << elapsed * 1000 / timer_precision() << " ms" << std::endl;
	if( time_limit > elapsed ) {
		state.bonus_time = (time_limit - elapsed) / 2;
	}
	else {
		state.bonus_time = 0;

		unsigned long long actual_overhead = elapsed - time_limit;
		if( actual_overhead > state.internal_overhead ) {
			std::cerr << "Updating internal overhead from " << state.internal_overhead * 1000 / timer_precision() << " ms to " << actual_overhead * 1000 / timer_precision() << " ms " << std::endl;
			state.internal_overhead = actual_overhead;
		}
	}
	state.time_remaining -= elapsed;

	if( ponder ) {
		l.unlock();
		calc( state.p, state.c, m, res, 0, state.time_remaining, state.clock, state.seen, state.last_mate, *this );
	}
}


void xboard_thread::start()
{
	join();
	do_abort = false;
	abort = false;
	best_move.flags = 0;

	spawn();
}


move xboard_thread::stop()
{
	do_abort = true;
	abort = true;
	join();
	move m = best_move;
	best_move.flags = 0;

	return m;
}


void xboard_thread::on_new_best_move( position const& p, color::type c, int depth, int evaluation, unsigned long long nodes, pv_entry const* pv )
{
	scoped_lock lock( mtx );
	if( !abort ) {

		unsigned long long elapsed = ( get_time() - state.last_go_time ) * 100 / timer_precision();
		std::stringstream ss;
		ss << std::setw(2) << depth << " " << std::setw(7) << evaluation << " " << std::setw(6) << elapsed << " " << std::setw(10) << nodes << " " << std::setw(0) << pv_to_string( pv, p, c ) << std::endl;
		if( state.post ) {
			std::cout << ss.str();
		}
		else {
			std::cerr << ss.str();
		}

		best_move = pv->get_best_move();
	}
}


void go( xboard_thread& thread, xboard_state& state, unsigned long long cmd_recv_time )
{
	state.last_go_time = cmd_recv_time;
	state.last_go_color = state.c;
	++state.moves_between_updates;

	if( !state.hash_initialized ) {
		transposition_table.init( conf.memory );
		state.hash_initialized = true;
	}
	// Do a step
	if( (state.in_book || state.clock < 30) && state.started_from_root ) {
		std::vector<book_entry> moves = state.book_.get_entries( state.p, state.c, state.move_history_, -1, true );
		if( moves.empty() ) {
			if( state.in_book ) {
				std::cerr << "Current position not in book" << std::endl;
				state.in_book = false;
			}
		}
		else {
			short best = moves.front().forecast;
			int count_best = 1;
			std::cerr << "Entries from book: " << std::endl;
			std::cerr << move_to_string( state.p, state.c, moves.front().m ) << " " << moves.front().forecast << " (" << moves.front().search_depth << ")" << std::endl;
			for( std::vector<book_entry>::const_iterator it = moves.begin() + 1; it != moves.end(); ++it ) {
				if( it->forecast > -33 && it->forecast + 25 >= best && count_best < 3 ) {
					++count_best;
				}
				std::cerr << move_to_string( state.p, state.c, it->m ) << " " << it->forecast << " (" << it->search_depth << ")" << std::endl;
			}

			book_entry best_move = moves[get_random_unsigned_long_long() % count_best];

			std::cout << "move " << move_to_string( state.p, state.c, best_move.m ) << std::endl;

			state.history.push_back( state.p );

			apply_move( state.p, best_move.m, state.c );
			++state.clock;
			state.c = static_cast<color::type>( 1 - state.c );
			state.move_history_.push_back( best_move.m );

			unsigned long long stop = get_time();
			state.time_remaining -= stop - state.last_go_time;
			std::cerr << "Elapsed: " << (stop - state.last_go_time) * 1000 / timer_precision() << " ms" << std::endl;
			return;
		}
	}

	if( state.clock < 21 && state.started_from_root ) {
		state.book_.mark_for_processing( state.move_history_ );
	}

	thread.start();
}

void xboard()
{
	xboard_state state;
	xboard_thread thread( state );

	if( conf.depth == -1 ) {
		conf.depth = 40;
	}

	pawn_hash_table.init( PAWN_HASH_TABLE_SIZE );

	while( true ) {
		std::string line;
		std::getline( std::cin, line );

		unsigned long long cmd_recv_time = get_time();

		if( !std::cin ) {
			std::cerr << "EOF" << std::endl;
			break;
		}
		if( line.empty() ) {
			continue;
		}

		logger::log_input( line );

		move best_move = thread.stop();

		scoped_lock l( thread.mtx );

		if( line == "xboard" ) {
			std::cout << std::endl;
		}
		else if( line == "quit" ) {
			break;
		}
		else if( line == "?" ) {
			if( best_move.flags & move_flags::valid ) {
				std::cout << "move " << move_to_string( state.p, state.c, best_move ) << std::endl;
				state.apply( best_move );
			}
			else {
				std::cout << "Error (command not legal now): ?" << std::endl;
			}
		}
		else if( line.substr( 0, 9 ) == "protover " ) {
			//std::cout << "feature ping=1" << std::endl;
			std::cout << "feature setboard=1" << std::endl;
			std::cout << "feature done=1" << std::endl;
		}
		else if( line.substr( 0, 7 ) == "result " ) {
			// Ignore
		}
		else if( line == "new" ) {
			state.reset();
		}
		else if( line == "force" ) {
			state.force = true;
		}
		else if( line == "random" ) {
			// Ignore
		}
		else if( line == "hard" ) {
			// Ignore
		}
		else if( line == "easy" ) {
			// Ignore
		}
		else if( line == "post" ) {
			state.post = true;
		}
		else if( line == "nopost" ) {
			state.post = false;
		}
		else if( line.substr( 0, 9 ) == "accepted " ) {
			// Ignore
		}
		else if( line.substr( 0, 9 ) == "rejected " ) {
			// Ignore
		}
		else if( line == "computer" ) {
			// Ignore
		}
		else if( line == "white" ) {
			state.c = color::white;
			state.self = color::black;
		}
		else if( line == "black" ) {
			state.c = color::black;
			state.self = color::white;
		}
		else if( line == "undo" ) {
			if( !state.undo(1) ) {
				std::cout << "Error (command not legal now): undo" << std::endl;
			}
		}
		else if( line == "remove" ) {
			if( !state.undo(2) ) {
				std::cout << "Error (command not legal now): undo" << std::endl;
			}
		}
		else if( line.substr( 0, 5 ) == "otim " ) {
			// Ignore
		}
		else if( line.substr( 0, 5 ) == "time " ) {
			line = line.substr( 5 );
			std::stringstream ss;
			ss.flags(std::stringstream::skipws);
			ss.str(line);

			unsigned long long t;
			ss >> t;
			if( !ss ) {
				std::cout << "Error (bad command): Not a valid time command" << std::endl;
			}
			else {
				state.time_remaining = static_cast<unsigned long long>(t) * timer_precision() / 100;
			}
		}
		else if( line.substr( 0, 6 ) == "level " ) {
			line = line.substr( 6 );
			std::stringstream ss;
			ss.flags(std::stringstream::skipws);
			ss.str(line);

			int control;
			ss >> control;

			std::string time;
			ss >> time;

			int increment;
			ss >> increment;

			if( !ss ) {
				std::cout << "Error (bad command): Not a valid level command" << std::endl;
				continue;
			}

			std::stringstream ss2;
			ss2.str(time);

			unsigned int minutes;
			unsigned int seconds = 0;

			ss2 >> minutes;
			if( !ss2 ) {
				std::cout << "Error (bad command): Not a valid level command" << std::endl;
				continue;
			}

			char ch;
			if( ss2 >> ch ) {
				if( ch == ':' ) {
					ss2 >> seconds;
					if( !ss2 ) {
						std::cout << "Error (bad command): Not a valid level command" << std::endl;
						continue;
					}
				}
				else {
					std::cout << "Error (bad command): Not a valid level command" << std::endl;
					continue;
				}
			}

			state.update_comm_overhead( (minutes * 60 + seconds) * timer_precision() );

			state.time_control = control;
			state.time_remaining = minutes * 60 + seconds;
			state.time_remaining *= timer_precision();
			state.time_increment = increment * timer_precision();
		}
		else if( line == "go" ) {
			state.force = false;
			state.self = state.c;
			// TODO: clocks...
			go( thread, state, cmd_recv_time );
		}
		else if( line == "~moves" ) {
			check_map check;
			calc_check_map( state.p, state.c, check );

			move_info moves[200];
			move_info* pm = moves;
			calculate_moves( state.p, state.c, pm, check );

			std::cout << "Possible moves:" << std::endl;
			move_info* it = &moves[0];
			for( ; it != pm; ++it ) {
				std::cout << " " << move_to_string( state.p, state.c, it->m ) << std::endl;
			}
		}
		else if( line.substr( 0, 4 ) == "~fen" || line.substr( 0, 8 ) == "setboard" ) {
			if( line.substr( 0, 4 ) == "~fen" ) {
				line = line.substr( 5 );
			}
			else {
				line = line.substr( 9 );
			}
			position new_pos;
			color::type new_c;
			if( !parse_fen_noclock( line, new_pos, new_c ) ) {
				std::cout << "Error (bad command): Not a valid FEN position" << std::endl;
				continue;
			}
			state.reset();
			state.p = new_pos;
			state.c = new_c;
			state.seen.pos[0] = get_zobrist_hash( state.p );
			state.in_book = false;
			state.started_from_root = false;
		}
		else if( line == "~score") {
			short eval = evaluate_full( state.p, state.c );
			std::cout << eval << std::endl;
		}
		else if( line == "~hash") {
			std::cout << get_zobrist_hash( state.p ) << std::endl;
		}
		else {
			move m;
			if( parse_move( state.p, state.c, line, m ) ) {

				state.apply( m );
				if( !state.force && state.c == state.self ) {
					go( thread, state, cmd_recv_time );
				}
			}
		}
	}
}


int main( int argc, char const* argv[] )
{
	std::string self = argv[0];
#if _MSC_VER
	std::replace( self.begin(), self.end(), '\\', '/' );
#endif
	if( self.rfind('/') != std::string::npos ) {
		book_dir = self.substr( 0, self.rfind('/') + 1 );
	}
#if _MSC_VER
	if( GetFileAttributes( (book_dir + "/opening_book.db").c_str() ) == INVALID_FILE_ATTRIBUTES ) {
		char buffer[MAX_PATH];
		GetModuleFileName( 0, buffer, MAX_PATH );
		buffer[MAX_PATH - 1] = 0;
		book_dir = buffer;
		std::replace( book_dir.begin(), book_dir.end(), '\\', '/' );
		if( book_dir.rfind('/') != std::string::npos ) {
			book_dir = book_dir.substr( 0, book_dir.rfind('/') + 1 );
		}
	}
#endif

	console_init();

	int i = conf.init( argc, argv );
	logger::init( conf.logfile );

	std::cerr << "  Octochess" << std::endl;
	std::cerr << "  ---------" << std::endl;
	std::cerr << std::endl;

	if( conf.random_seed != -1 ) {
		init_random( conf.random_seed );
	}
	else {
		unsigned long long seed = get_time();
		init_random(seed);
		std::cerr << "Random seed is " << seed << std::endl;
	}
	init_zobrist_tables();

	if( i < argc && !strcmp(argv[i], "auto" ) ) {
		auto_play();
	}
	else if( i < argc && !strcmp(argv[i], "perft" ) ) {
		perft();
	}
	else if( i < argc && !strcmp(argv[i], "test" ) ) {
		selftest();
	}
	else if( i < argc && !strcmp(argv[i], "tweakgen" ) ) {
		generate_test_positions();
	}
	else if( i < argc && !strcmp(argv[i], "tweak" ) ) {
		tweak_evaluation();
	}
	else {
		xboard();
	}

	logger::cleanup();
}
