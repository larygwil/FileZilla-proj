#include "chess.hpp"
#include "xboard.hpp"
#include "book.hpp"
#include "calc.hpp"
#include "eval.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "logger.hpp"
#include "pawn_structure_hash_table.hpp"
#include "random.hpp"
#include "see.hpp"
#include "string.hpp"
#include "time.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

namespace mode {
enum type {
	force,
	normal,
	analyze
};
}

struct xboard_state
{
	xboard_state()
		: c()
		, clock()
		, book_( conf.book_dir )
		, time_remaining()
		, bonus_time()
		, mode_(mode::force)
		, self(color::black)
		, hash_initialized()
		, time_control()
		, time_increment()
		, history()
		, post(true)
		, last_mate()
		, started_from_root()
		, internal_overhead( duration::milliseconds(100) )
		, communication_overhead( duration::milliseconds(50) )
		, last_go_color()
		, moves_between_updates()
		, level_cmd_count()
	{
		reset();
	}

	void reset()
	{
		init_board(p);
		if( conf.use_book && book_.is_open() ) {
			std::cerr << "Opening book loaded" << std::endl;
		}
		move_history_.clear();

		c = color::white;

		clock = 1;

		seen.reset_root( get_zobrist_hash( p ) );

		time_remaining = conf.time_limit;
		bonus_time = duration();

		mode_ = mode::normal;
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
			seen.push_root( get_zobrist_hash( p ) );
		}
		else {
			seen.reset_root( get_zobrist_hash( p ) );
		}

		if( seen.depth() >= 110 ) {
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

		// This isn't exactly correct, if popping past root we would need to restore old seen state prior to a reset.
		seen.pop_root( count );

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

		return true;
	}

	void update_comm_overhead( duration const& new_remaining )
	{
		if( c == last_go_color && moves_between_updates && (time_remaining - new_remaining).absolute() < duration::seconds(2 * moves_between_updates) ) {
			level_cmd_differences += time_remaining - new_remaining;
			level_cmd_count += moves_between_updates;

			duration comm_overhead;
			if( level_cmd_differences >= duration() ) {
				comm_overhead = level_cmd_differences / level_cmd_count;
			}
			else {
				comm_overhead = duration();
			}

			std::cerr << "Updating communication overhead from " << communication_overhead.milliseconds() << " ms to " << comm_overhead.milliseconds() << " ms " << std::endl;
			communication_overhead = comm_overhead;
		}

		moves_between_updates = 0;
	}

	position p;
	color::type c;
	int clock;
	seen_positions seen;
	book book_;
	duration time_remaining;
	duration bonus_time;
	mode::type mode_;
	color::type self;
	bool hash_initialized;
	uint64_t time_control;
	duration time_increment;

	std::list<position> history;
	std::vector<move> move_history_;

	bool post;

	short last_mate;

	bool started_from_root;

	// If we calculate move time of x but consume y > x amount of time, internal overhead if y - x.
	// This is measured locally between receiving the go and sending out the reply.
	duration internal_overhead;

	// If we receive time updates between moves, communication_overhead is the >=0 difference between two timer updates
	// and the calculated time consumption.
	duration communication_overhead;
	timestamp last_go_time;

	color::type last_go_color;
	unsigned int moves_between_updates;

	// Level command is in seconds only. Hence we need to accumulate data before we can update the
	// communication overhead.
	duration level_cmd_differences;
	uint64_t level_cmd_count;
};


volatile extern bool do_abort;

class xboard_thread : public thread, public new_best_move_callback
{
public:
	xboard_thread( xboard_state& s );
	~xboard_thread();

	virtual void onRun();

	void start( bool just_ponder = false );

	move stop();

	mutex mtx;

	virtual void on_new_best_move( position const& p, color::type c, int depth, int evaluation, uint64_t nodes, pv_entry const* pv );

private:
	calc_manager cmgr_;
	bool abort;
	xboard_state& state;
	move best_move;
	bool ponder_;
};


xboard_thread::xboard_thread( xboard_state& s )
: abort()
, state(s)
, ponder_()
{
}


xboard_thread::~xboard_thread()
{
	stop();
}


void xboard_thread::onRun()
{
	if( !ponder_ ) {
		if( state.bonus_time > state.time_remaining ) {
			state.bonus_time = duration();
		}

		uint64_t remaining_moves;
		if( !state.time_control ) {
			remaining_moves = (std::max)( 20, (80 - state.clock) / 2 );
		}
		else {
			remaining_moves = (state.time_control * 2) - (state.clock % (state.time_control * 2));
		}
		duration time_limit = (state.time_remaining - state.bonus_time) / remaining_moves + state.bonus_time;
		duration overhead = state.internal_overhead + state.communication_overhead;

		if( !state.time_increment.empty() && state.time_remaining > (time_limit + state.time_increment) ) {
			time_limit += state.time_increment;
		}

		if( time_limit > overhead ) {
			time_limit -= overhead;
		}
		else {
			time_limit = duration();
		}

		// Any less time makes no sense.
		if( time_limit < duration::milliseconds(10) ) {
			time_limit = duration::milliseconds(10);
		}

		move m;
		int res;
		bool success = cmgr_.calc( state.p, state.c, m, res, time_limit, state.clock, state.seen, state.last_mate, *this );

		scoped_lock l( mtx );

		if( abort ) {
			return;
		}

		if( success ) {

			std::cout << "move " << move_to_string( m ) << std::endl;

			state.apply( m );

			{
				score base_eval = state.c ? -state.p.base_eval : state.p.base_eval;
				std::cerr << "  ; Current base evaluation: " << base_eval << " centipawns, forecast " << res << std::endl;
			}

			if( res > result::win_threshold ) {
				state.last_mate = res;
			}
			else {
				ponder_ = conf.ponder;
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
		timestamp stop;
		duration elapsed = timestamp() - state.last_go_time;

		std::cerr << "Elapsed: " << elapsed.milliseconds() << " ms" << std::endl;
		if( time_limit > elapsed ) {
			state.bonus_time = (time_limit - elapsed) / 2;
		}
		else {
			state.bonus_time = duration();

			duration actual_overhead = elapsed - time_limit;
			if( actual_overhead > state.internal_overhead ) {
				std::cerr << "Updating internal overhead from " << state.internal_overhead.milliseconds() << " ms to " << actual_overhead.milliseconds() << " ms " << std::endl;
				state.internal_overhead = actual_overhead;
			}
		}
		state.time_remaining -= elapsed;
	}

	if( ponder_ ) {
		move m;
		int res;
		cmgr_.calc( state.p, state.c, m, res, duration::infinity(), state.clock, state.seen, state.last_mate, *this );
	}
}


void xboard_thread::start( bool just_ponder )
{
	join();
	do_abort = false;
	abort = false;
	best_move.piece = pieces::none;
	ponder_ = just_ponder;

	spawn();
}


move xboard_thread::stop()
{
	do_abort = true;
	abort = true;
	join();
	move m = best_move;
	best_move.piece = pieces::none;

	return m;
}


void xboard_thread::on_new_best_move( position const& p, color::type c, int depth, int evaluation, uint64_t nodes, pv_entry const* pv )
{
	scoped_lock lock( mtx );
	if( !abort ) {

		int64_t elapsed = ( timestamp() - state.last_go_time ).milliseconds() / 10;
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


void go( xboard_thread& thread, xboard_state& state, timestamp const& cmd_recv_time )
{
	state.last_go_time = cmd_recv_time;
	state.last_go_color = state.c;
	++state.moves_between_updates;

	if( !state.hash_initialized ) {
		transposition_table.init( conf.memory );
		state.hash_initialized = true;
	}
	// Do a step
	if( conf.use_book && state.book_.is_open() && state.clock < 30 && state.started_from_root ) {
		std::vector<book_entry> moves = state.book_.get_entries( state.p, state.c, state.move_history_, -1, true );
		if( moves.empty() ) {
			std::cerr << "Current position not in book" << std::endl;
		}
		else {
			std::cerr << "Entries from book: " << std::endl;
			std::cerr << entries_to_string( moves );

			short best = moves.front().folded_forecast;
			int count_best = 1;
			for( std::vector<book_entry>::const_iterator it = moves.begin() + 1; it != moves.end(); ++it ) {
				if( it->folded_forecast > -33 && it->folded_forecast + 25 >= best && count_best < 3 ) {
					++count_best;
				}
			}

			book_entry best_move = moves[get_random_unsigned_long_long() % count_best];

			std::cout << "move " << move_to_string( best_move.m ) << std::endl;

			state.history.push_back( state.p );

			apply_move( state.p, best_move.m, state.c );
			++state.clock;
			state.c = static_cast<color::type>( 1 - state.c );
			state.move_history_.push_back( best_move.m );

			timestamp stop;
			state.time_remaining -= stop - state.last_go_time;
			std::cerr << "Elapsed: " << (stop - state.last_go_time).milliseconds() << " ms" << std::endl;
			return;
		}
	}

	if( state.clock < 22 && state.started_from_root ) {
		state.book_.mark_for_processing( state.move_history_ );
	}

	thread.start();
}

void xboard( std::string line)
{
	xboard_state state;
	xboard_thread thread( state );

	if( conf.depth == -1 ) {
		conf.depth = MAX_DEPTH;
	}

	pawn_hash_table.init( conf.pawn_hash_table_size );

	if( !line.empty() ) {
		goto skip_getline;
	}

	while( true ) {
		std::getline( std::cin, line );
skip_getline:

		timestamp cmd_recv_time;

		if( !std::cin ) {
			std::cerr << "EOF" << std::endl;
			break;
		}
		if( line.empty() ) {
			continue;
		}

		std::string args;
		std::string cmd = split( line, args );

		logger::log_input( line );

		// The following two commands do not stop the thread.
		if( cmd == "hard" ) {
			scoped_lock l( thread.mtx );
			conf.ponder = true;
			continue;
		}
		else if( cmd == "easy" ) {
			scoped_lock l( thread.mtx );
			conf.ponder = false;
			continue;
		}
		else if( cmd == "." ) {
			scoped_lock l( thread.mtx );
			// TODO: Implement
			std::cout << "Error (unknown command): .";
			continue;
		}

		move best_move = thread.stop();

		scoped_lock l( thread.mtx );

		if( cmd == "xboard" ) {
			std::cout << std::endl;
		}
		else if( cmd == "quit" ) {
			break;
		}
		else if( cmd == "?" ) {
			if( !best_move.empty() ) {
				std::cout << "move " << move_to_string( best_move ) << std::endl;
				state.apply( best_move );
			}
			else {
				std::cout << "Error (command not legal now): ?" << std::endl;
			}
		}
		else if( cmd == "protover" ) {
			//std::cout << "feature ping=1" << std::endl;
			std::cout << "feature analyze=1" << std::endl;
			std::cout << "feature myname=\"Octochess\"" << std::endl;
			std::cout << "feature setboard=1" << std::endl;
			std::cout << "feature sigint=0" << std::endl;
			std::cout << "feature variants=\"normal\"" << std::endl;

			//std::cout << "feature option=\"Apply -save\"" << std::endl;
			//std::cout << "feature option=\"Defaults -reset\"" << std::endl;
			//std::cout << "feature option=\"Maximum search depth -spin " << static_cast<int>(conf.depth) << " 1 40\"" << std::endl;

			std::cout << "feature done=1" << std::endl;
		}
		else if( cmd == "result" ) {
			// Ignore
		}
		else if( cmd == "new" ) {
			bool analyze = state.mode_ == mode::analyze;
			state.reset();
			if( analyze ) {
				state.mode_ = mode::analyze;
				thread.start( true );
			}
		}
		else if( cmd == "force" ) {
			state.mode_ = mode::force;
		}
		else if( cmd == "random" ) {
			// Ignore
		}
		else if( cmd == "post" ) {
			state.post = true;
		}
		else if( cmd == "nopost" ) {
			state.post = false;
		}
		else if( cmd == "accepted" ) {
			// Ignore
		}
		else if( cmd == "rejected" ) {
			// Ignore
		}
		else if( cmd == "computer" ) {
			// Ignore
		}
		else if( cmd == "white" ) {
			state.c = color::white;
			state.self = color::black;
		}
		else if( cmd == "black" ) {
			state.c = color::black;
			state.self = color::white;
		}
		else if( cmd == "undo" ) {
			if( !state.undo(1) ) {
				std::cout << "Error (command not legal now): undo" << std::endl;
			}
			else {
				if( state.mode_ == mode::analyze ) {
					thread.start( true );
				}
			}
		}
		else if( cmd == "remove" ) {
			if( !state.undo(2) ) {
				std::cout << "Error (command not legal now): remove" << std::endl;
			}
			else {
				if( state.mode_ == mode::analyze ) {
					thread.start( true );
				}
			}
		}
		else if( cmd == "otim" ) {
			// Ignore
		}
		else if( cmd == "time" ) {
			std::stringstream ss;
			ss.flags(std::stringstream::skipws);
			ss.str(args);

			uint64_t t;
			ss >> t;
			if( !ss ) {
				std::cout << "Error (bad command): Not a valid time command" << std::endl;
			}
			else {
				state.time_remaining = duration::milliseconds( t * 10 );
			}
		}
		else if( cmd == "level" ) {
			std::stringstream ss;
			ss.flags(std::stringstream::skipws);
			ss.str(args);

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

			state.update_comm_overhead( duration::minutes(minutes) + duration::seconds(seconds) );

			state.time_control = control;
			state.time_remaining = duration::minutes(minutes) + duration::seconds(seconds);
			state.time_increment = duration::seconds(increment);
		}
		else if( cmd == "go" ) {
			state.mode_ = mode::normal;
			state.self = state.c;
			// TODO: clocks...
			go( thread, state, cmd_recv_time );
		}
		else if( cmd == "analyze" ) {
			state.mode_ = mode::analyze;
			if( !state.hash_initialized ) {
				transposition_table.init( conf.memory );
				state.hash_initialized = true;
			}
			thread.start( true );
		}
		else if( cmd == "exit" ) {
			state.mode_ = mode::normal;
			state.moves_between_updates = 0;
		}
		else if( cmd == "~moves" ) {
			check_map check( state.p, state.c );

			move_info moves[200];
			move_info* pm = moves;
			calculate_moves( state.p, state.c, pm, check );

			std::cout << "Possible moves:" << std::endl;
			move_info* it = &moves[0];
			for( ; it != pm; ++it ) {
				std::cout << " " << move_to_string( it->m ) << std::endl;
			}
		}
		else if( cmd == "~fen" ) {
			std::cout << position_to_fen_noclock( state.p, state.c ) << std::endl;
		}
		else if( cmd == "setboard" ) {
			position new_pos;
			color::type new_c;
			std::string error;
			if( !parse_fen_noclock( args, new_pos, new_c, &error ) ) {
				std::cout << "Error (bad command): Not a valid FEN position: " << error << std::endl;
				continue;
			}
			bool analyze = state.mode_ == mode::analyze;

			state.reset();
			state.p = new_pos;
			state.c = new_c;
			state.seen.reset_root( get_zobrist_hash( state.p ) );
			state.started_from_root = false;

			if( analyze ) {
				state.mode_ = mode::analyze;
				thread.start( true );
			}
		}
		else if( cmd == "~score") {
			std::cout << explain_eval( state.p, state.c ) << std::endl;
		}
		else if( cmd == "~hash") {
			std::cout << get_zobrist_hash( state.p ) << std::endl;
		}
		else if( cmd == "~see" ) {
			move m;
			if( parse_move( state.p, state.c, args, m, true ) ) {
				if( m.captured_piece != pieces::none ) {
					int see_score = see( state.p, state.c, m );
					std::cout << "See score: " << see_score << std::endl;
				}
				else {
					std::cerr << "Not a capture move" << std::endl;
				}
			}
		}
		else {
			move m;
			if( parse_move( state.p, state.c, line, m ) ) {

				state.apply( m );
				if( state.mode_ == mode::normal && state.c == state.self ) {
					go( thread, state, cmd_recv_time );
				}
				else if( state.mode_ == mode::analyze ) {
					thread.start( true );
				}
			}
		}
	}
}
