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
#include "hash.hpp"
#include "mobility.hpp"
#include "moves.hpp"
#include "util.hpp"
#include "pawn_structure_hash_table.hpp"
#include "platform.hpp"
#include "statistics.hpp"
#include "zobrist.hpp"
#include "logger.hpp"

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
	int res;

	seen_positions seen;
	seen.root_position = 0;
	seen.pos[0] = get_zobrist_hash( p, c );

	while( calc( p, c, m, res, TIME_LIMIT * timer_precision() / 1000, TIME_LIMIT * timer_precision() / 1000, i, seen ) ) {
		if( c == color::white ) {
			std::cout << std::setw(3) << i << ".";
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

		bool captured;
		apply_move( p, m, c, captured );
		int ev = evaluate_fast( p, color::white );
		std::cerr << "Evaluation (for white): " << ev << " centipawns" << std::endl;

		//std::cerr << explain_eval( p, color::white, p.bitboards );

		c = static_cast<color::type>(1-c);

		if( !reset_seen ) {
			seen.pos[++seen.root_position] = get_zobrist_hash( p, c );
		}
		else {
			seen.root_position = 0;
			seen.pos[0] = get_zobrist_hash( p, c );
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
		, in_book()
		, book_index()
		, time_remaining()
		, bonus_time()
		, force(true)
		, self(color::black)
		, hash_initialized()
		, time_control()
		, time_increment()
		, history()
		, post(true)
	{
		reset();
	}

	void reset()
	{
		init_board(p);
		in_book = open_book( book_dir );
		if( in_book ) {
			std::cerr << "Opening book loaded" << std::endl;
		}
		book_index = 0;

		c = color::white;

		clock = 1;

		seen = seen_positions();
		seen.root_position = 0;
		seen.pos[0] = get_zobrist_hash( p, c );

		time_remaining = conf.time_limit * timer_precision() / 1000;
		bonus_time = 0;

		force = false;
		self = color::black;
	}

	void apply( move const& m )
	{
		history.push_back( p );
		bool reset_seen = false;
		if( m.piece == pieces::pawn || m.captured_piece ) {
			reset_seen = true;
		}

		bool captured;
		apply_move( p, m, c, captured );
		++clock;
		c = static_cast<color::type>( 1 - c );

		if( !reset_seen ) {
			seen.pos[++seen.root_position] = get_zobrist_hash( p, c );
		}
		else {
			seen.root_position = 0;
			seen.pos[0] = get_zobrist_hash( p, c );
		}

		if( seen.root_position >= 110 ) {
			std::cout << "1/2-1/2 (Draw)" << std::endl;
		}

		if( in_book ) {
			std::vector<move_entry> moves;
			/*book_entry entry = */
				get_entries( book_index, moves );
			if( moves.empty() ) {
				in_book = false;
			}
			else {
				in_book = false;
				for( std::vector<move_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
					if( it->get_move() == m ) {
						if( it->next_index ) {
							in_book = true;
							book_index = it->next_index;
						}
						break;
					}
				}
			}
			if( !in_book ) {
				std::cerr << "Left opening book" << std::endl;
			}
		}
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
		}
		p = history.back();
		history.pop_back();
		

		bonus_time = false;
		in_book = false;

		return true;
	}

	position p;
	color::type c;
	int clock;
	seen_positions seen;
	bool in_book;
	unsigned long long book_index;
	unsigned long long time_remaining;
	unsigned long long bonus_time;
	bool force;
	color::type self;
	bool hash_initialized;
	unsigned long long time_control;
	unsigned long long time_increment;

	std::list<position> history;

	bool post;
};


volatile extern bool do_abort;

class xboard_thread : public thread, public new_best_move_callback
{
public:
	xboard_thread( xboard_state& s );
	~xboard_thread();

	virtual void onRun();

	void start( unsigned long long t );

	move stop();

	mutex mtx;

	virtual void on_new_best_move( position const& p, color::type c, int depth, int evaluation, unsigned long long nodes, pv_entry const* pv );

private:
	bool abort;
	xboard_state& state;
	unsigned long long starttime;
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
	unsigned long long overhead_compensation = 100 * timer_precision() / 1000;

	if( state.time_increment && state.time_remaining > (time_limit + state.time_increment) ) {
		time_limit += state.time_increment;
	}

	if( time_limit > overhead_compensation ) {
		time_limit -= overhead_compensation;
	}

	move m;
	int res;
	bool success = calc( state.p, state.c, m, res, time_limit, state.time_remaining, state.clock, state.seen, *this );
	
	scoped_lock l( mtx );

	if( abort ) {
		return;
	}

	if( success ) {

		std::cout << "move " << move_to_string( state.p, state.c, m ) << std::endl;

		state.apply( m );

		{
			int i = evaluate_fast( state.p, static_cast<color::type>(1-state.c) );
			std::cerr << "  ; Current evaluation: " << i << " centipawns, forecast " << res << std::endl;
			
			//std::cerr << explain_eval( state.p, static_cast<color::type>(1-state.c), p.bitboards );
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
	unsigned long long elapsed = stop - starttime;
	if( time_limit > elapsed ) {
		state.bonus_time = (time_limit - elapsed) / 2;
	}
	else {
		state.bonus_time = 0;
	}
	state.time_remaining -= elapsed;
}


void xboard_thread::start( unsigned long long t )
{
	join();
	do_abort = false;
	abort = false;
	best_move.flags = 0;

	starttime = t;

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

		unsigned long long elapsed = ( get_time() - starttime ) * 100 / timer_precision();
		std::stringstream ss;
		ss << std::setw(2) << depth << " " << std::setw(7) << evaluation << " " << std::setw(10) << elapsed << " " << nodes << " " << std::setw(0) << pv_to_string( pv, p, c ) << std::endl;
		if( state.post ) {
			std::cout << ss.str();
		}
		else {
			std::cerr << ss.str();
		}

		best_move = pv->get_best_move();
	}
}


void go( xboard_thread& thread, xboard_state& state )
{
	unsigned long long start = get_time();
	if( !state.hash_initialized ) {
		transposition_table.init( conf.memory );
		state.hash_initialized = true;
	}
	// Do a step
	if( state.in_book ) {
		std::vector<move_entry> moves;
		/*book_entry entry = */
			get_entries( state.book_index, moves );
		if( moves.empty() ) {
			state.in_book = false;
		}
		else {
			short best = moves.front().forecast;
			int count_best = 0;
			std::cerr << "Entries from book: " << std::endl;
			for( std::vector<move_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
				if( it->forecast + 25 >= best ) {
					++count_best;
				}
				std::cerr << move_to_string( state.p, state.c, it->get_move() ) << " " << it->forecast << std::endl;
			}

			move_entry best_move = moves[get_random_unsigned_long_long() % count_best];
			move m = best_move.get_move();
			if( !best_move.next_index ) {
				state.in_book = false;
				std::cerr << "Left opening book" << std::endl;
			}
			else {
				state.book_index = best_move.next_index;
			}

			std::cout << "move " << move_to_string( state.p, state.c, m ) << std::endl;

			state.history.push_back( state.p );

			bool captured;
			apply_move( state.p, m, state.c, captured );
			++state.clock;
			state.c = static_cast<color::type>( 1 - state.c );

			unsigned long long stop = get_time();
			state.time_remaining -= stop - start;
			return;
		}
	}

	thread.start( start );
}

void xboard()
{
	xboard_state state;
	xboard_thread thread( state );

	std::string line;
	std::getline( std::cin, line );
	logger::log_input( line );
	if( line != "xboard" ) {
		std::cerr << "First command needs to be xboard!" << std::endl;
		exit(1);
	}

	if( conf.depth == -1 ) {
		conf.depth = 40;
	}

	std::cout << std::endl;

	pawn_hash_table.init( PAWN_HASH_TABLE_SIZE );

	while( true ) {
		std::getline( std::cin, line );
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

		if( line == "quit" ) {
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

			state.time_control = control;
			state.time_remaining = minutes * 60 + seconds;
			state.time_remaining *= timer_precision();
			state.time_increment = increment * timer_precision();
		}
		else if( line == "go" ) {
			state.force = false;
			state.self = state.c;
			// TODO: clocks...
			go( thread, state );
		}
		else if( line == "~moves" ) {
			check_map check;
			calc_check_map( state.p, state.c, check );

			move_info moves[200];
			move_info* pm = moves;
			calculate_moves( state.p, state.c, 0, pm, check, killer_moves() );

			std::cout << "Possible moves:" << std::endl;
			move_info* it = &moves[0];
			for( ; it != pm; ++it ) {
				std::cout << " " << move_to_string( state.p, state.c, it->m ) << std::endl;
			}
		}
		else {
			move m;
			if( parse_move( state.p, state.c, line, m ) ) {

				state.apply( m );
				if( !state.force && state.c == state.self ) {
					go( thread, state );
				}
			}
		}
	}
}


void perft( int depth, position const& p, color::type c, unsigned long long& n )
{
	if( conf.depth == -1 ) {
		conf.depth = 8;
	}

	if( !depth-- ) {
		++n;
		return;
	}

	move_info moves[200];
	move_info* pm = moves;

	check_map check;
	calc_check_map( p, c, check );
	calculate_moves( p, c, 0, pm, check, killer_moves() );

	for( move_info* it = moves; it != pm; ++it ) {
		position new_pos = p;
		bool captured;
		apply_move( new_pos, *it, c, captured );
		perft( depth, new_pos, static_cast<color::type>(1-c), n );
	}

}

void perft()
{
	pawn_hash_table.init( PAWN_HASH_TABLE_SIZE );

	unsigned long long const perft_results[] = {
		20ull,
		400ull,
		8902ull,
		197281ull,
		4865609ull,
		119060324ull,
		3195901860ull,
		84998978956ull
	};

	for( unsigned int i = 0; i < sizeof(perft_results)/sizeof(unsigned long long); ++i ) {
		std::cerr << "Calculating number of possible moves in " << (i + 1) << " plies:" << std::endl;

		position p;
		init_board( p );

		unsigned long long ret = 0;

		int max_depth = i + 1;

		unsigned long long start = get_time();
		perft( max_depth, p, color::white, ret );
		unsigned long long stop = get_time();


		std::cerr << "Moves: "     << ret << std::endl;
		std::cerr << "Took:  "     << (stop - start) * 1000 / timer_precision() << " ms" << std::endl;
		std::cerr << "Time/move: " << ((stop - start) * 1000 * 1000 * 1000) / ret / timer_precision() << " ns" << std::endl;

		if( ret != perft_results[i] ) {
			std::cerr << "FAIL! Expected " << perft_results[i] << " moves." << std::endl;
			break;
		}
		else {
			std::cerr << "PASS" << std::endl;
		}
		std::cerr << std::endl;
	}
	
}


int main( int argc, char const* argv[] )
{
	std::string self = argv[0];
	if( self.rfind('/') != std::string::npos ) {
		book_dir = self.substr( 0, self.rfind('/') + 1 );
	}

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

	if( i < argc && !strcmp(argv[i], "xboard" ) ) {
		xboard();
	}
	else if( i < argc && !strcmp(argv[i], "perft" ) ) {
		perft();
	}
	else {
		auto_play();
	}

	logger::cleanup();
}
