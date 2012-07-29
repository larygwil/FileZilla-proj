#include "assert.hpp"
#include "chess.hpp"
#include "xboard.hpp"
#include "book.hpp"
#include "calc.hpp"
#include "eval.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "logger.hpp"
#include "pawn_structure_hash_table.hpp"
#include "pv_move_picker.hpp"
#include "random.hpp"
#include "see.hpp"
#include "statistics.hpp"
#include "util/mutex.hpp"
#include "util/string.hpp"
#include "util/thread.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

namespace mode {
enum type {
	force,
	normal,
	analyze,
	edit
};
}


struct xboard_state
{
	xboard_state()
		: clock()
		, book_( conf.book_dir )
		, time_remaining()
		, bonus_time()
		, mode_(mode::force)
		, self(color::black)
		, time_control()
		, time_increment()
		, history()
		, post(true)
		, last_mate()
		, started_from_root()
		, internal_overhead( duration::milliseconds(50) )
		, communication_overhead( duration::milliseconds(10) )
		, last_go_color()
		, moves_between_updates()
		, level_cmd_count()
	{
		reset();
	}

	void reset()
	{
		p.reset();
		if( conf.use_book && book_.is_open() ) {
			std::cerr << "Opening book loaded" << std::endl;
		}
		move_history_.clear();

		clock = 1;

		seen.reset_root( get_zobrist_hash( p ) );

		time_remaining = conf.time_limit;
		bonus_time = duration();
		fixed_move_time = duration();

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

		apply_move( p, m );
		++clock;

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
		if( p.self() == last_go_color && moves_between_updates && (time_remaining - new_remaining).absolute() < duration::seconds(2 * moves_between_updates) ) {
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

	// Returns false if program should quit.
	bool handle_edit_mode( std::string const& cmd );

	position p;
	int clock;
	seen_positions seen;
	book book_;
	duration time_remaining;
	duration bonus_time;
	mode::type mode_;
	color::type self;
	uint64_t time_control;
	duration time_increment;
	duration fixed_move_time;

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

	pv_move_picker pv_move_picker_;
};


volatile extern bool do_abort;

class xboard_thread : public thread, public new_best_move_callback_base
{
public:
	xboard_thread( xboard_state& s );
	~xboard_thread();

	virtual void onRun();

	void start( bool just_ponder = false );

	move stop();

	mutex mtx;

	virtual void on_new_best_move( position const& p, int depth, int selective_depth, int evaluation, uint64_t nodes, duration const& elapsed, pv_entry const* pv );

private:
	calc_manager cmgr_;
	bool abort;
	xboard_state& state;
	move best_move;
	bool ponder_;
};


bool xboard_state::handle_edit_mode( std::string const& cmd )
{
	if( cmd == "c" ) {
		p.c = static_cast<color::type>(1-p.c);
	}
	else if( cmd == "#" ) {
		p.clear_bitboards();
	}
	else if( cmd == "." ) {
		p.c = static_cast<color::type>(clock);
		p.update_derived();
		p.can_en_passant = 0;

		p.castle[color::white] = 0;
		p.castle[color::black] = 0;
		if( p.king_pos[color::white] == 4 ) {
			if( p.bitboards[color::white].b[bb_type::rooks] & (1ull << 7) ) {
				p.castle[color::white] |= castles::kingside;
			}
			if( p.bitboards[color::white].b[bb_type::rooks] & 1ull ) {
				p.castle[color::white] |= castles::queenside;
			}
		}
		if( p.king_pos[color::black] == 60 ) {
			if( p.bitboards[color::black].b[bb_type::rooks] & (1ull << 63) ) {
				p.castle[color::black] |= castles::kingside;
			}
			if( p.bitboards[color::black].b[bb_type::rooks] & (1ull << 56) ) {
				p.castle[color::black] |= castles::queenside;
			}
		}

		move_history_.clear();
		seen.reset_root( get_zobrist_hash( p ) );
		started_from_root = false;

		if( !p.verify() ) {
			// Specs say to tell user error until next edit/new/setboard command. Since we cannot do that yet, refuse to leave edit mode.
			std::cout << "Error (invalid position): Cannot leave edit mode while the position is not valid." << std::endl;
		}
		else {
			clock = 1;
			// Not quite correct, but fits the idiom used by Win/XBoard
			mode_ = mode::force;
		}
	}
	else if( cmd == "quit" ) {
		return false;
	}
	else {
		if( cmd.size() == 3 ) {
			int rank = -1;
			int file = -1;
			if( cmd[1] >= 'a' && cmd[1] <= 'h' ) {
				file = cmd[1] - 'a';
			}
			else if( cmd[1] >= 'A' && cmd[1] <= 'H' ) {
				file = cmd[1] - 'H';
			}
			if( cmd[2] >= '1' && cmd[2] <= '8' ) {
				rank = cmd[2] - '1';
			}
			int piece = -1;
			switch( cmd[0] ) {
			case 'x':
			case 'X':
				piece = pieces::none;
				break;
			case 'p':
			case 'P':
				piece = pieces::pawn;
				break;
			case 'n':
			case 'N':
				piece = pieces::knight;
				break;
			case 'b':
			case 'B':
				piece = pieces::bishop;
				break;
			case 'r':
			case 'R':
				piece = pieces::rook;
				break;
			case 'q':
			case 'Q':
				piece = pieces::queen;
				break;
			case 'k':
			case 'K':
				piece = pieces::king;
				break;
			default:
				break;
			}
		
			if( piece != -1 && rank != -1 && file != -1 ) {
				int sq = static_cast<uint64_t>(file + rank * 8);
				for( int pi = bb_type::pawns; pi <= bb_type::king; ++pi ) {
					p.bitboards[p.self()].b[pi] &= ~(1ull << sq);
				}
				p.bitboards[p.self()].b[piece] |= 1ull << sq;
				return true;
			}
		}
		std::cout << "Error (bad command): Not a valid command in edit mode." << std::endl;
	}
	return true;
}


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
		duration time_limit;
		duration deadline;
		if( !state.fixed_move_time.empty() ) {
			time_limit = state.fixed_move_time;
			deadline = state.fixed_move_time;
		}
		else {
			if( state.bonus_time > state.time_remaining ) {
				state.bonus_time = duration();
			}

			uint64_t remaining_moves = std::max( 20, (80 - state.clock) / 2 );
			if( state.time_control ) {
				uint64_t remaining = (state.time_control * 2) - (state.clock % (state.time_control * 2));
				if( remaining < remaining_moves ) {
					remaining_moves = remaining;
				}
			}

			time_limit = (state.time_remaining - state.bonus_time) / remaining_moves + state.bonus_time;

			if( !state.time_increment.empty() && state.time_remaining > (time_limit + state.time_increment) ) {
				time_limit += state.time_increment;
			}

			deadline = state.time_remaining;
		}

		duration overhead = state.internal_overhead + state.communication_overhead;
		if( time_limit > overhead ) {
			time_limit -= overhead;
		}
		else {
			time_limit = duration();
		}
		if( deadline > overhead ) {
			deadline -= overhead;
		}
		else {
			deadline = duration();
		}

		// Any less time makes no sense.
		if( time_limit < duration::milliseconds(10) ) {
			time_limit = duration::milliseconds(10);
		}
		if( deadline < duration::milliseconds(10) ) {
			deadline = duration::milliseconds(10);
		}


		calc_result result = cmgr_.calc( state.p, time_limit, deadline, state.clock, state.seen, state.last_mate, *this );

		scoped_lock l( mtx );

		if( abort ) {
			return;
		}
		do_abort = false;

		if( !result.best_move.empty() ) {

			std::cout << "move " << move_to_string( result.best_move ) << std::endl;

			state.apply( result.best_move );

			{
				score base_eval = state.p.white() ? state.p.base_eval : -state.p.base_eval;
				std::cerr << "  ; Current base evaluation: " << base_eval << " centipawns, forecast " << result.forecast << std::endl;
			}

			if( result.forecast > result::win_threshold ) {
				state.last_mate = result.forecast;
			}
			else {
				ponder_ = conf.ponder;
			}
		}
		else {
			if( result.forecast == result::win ) {
				std::cout << "1-0 (White wins)" << std::endl;
			}
			else if( result.forecast == result::loss ) {
				std::cout << "0-1 (Black wins)" << std::endl;
			}
			else {
				std::cout << "1/2-1/2 (Draw)" << std::endl;
			}
		}
		duration elapsed = timestamp() - state.last_go_time;

		std::cerr << "Elapsed: " << elapsed.milliseconds() << " ms" << std::endl;
		if( time_limit > elapsed ) {
			state.bonus_time = (time_limit - elapsed) / 2;
		}
		else {
			state.bonus_time = duration();

			if( time_limit + result.used_extra_time > elapsed ) {
				duration actual_overhead = elapsed - time_limit - result.used_extra_time;
				if( actual_overhead > state.internal_overhead ) {
					std::cerr << "Updating internal overhead from " << state.internal_overhead.milliseconds() << " ms to " << actual_overhead.milliseconds() << " ms " << std::endl;
					state.internal_overhead = actual_overhead;
				}
			}
		}
		state.time_remaining -= elapsed;
	}

	if( ponder_ ) {
		cmgr_.calc( state.p, duration::infinity(), duration::infinity(), state.clock, state.seen, state.last_mate, *this );
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


void xboard_thread::on_new_best_move( position const& p, int depth, int /*selective_depth*/, int evaluation, uint64_t nodes, duration const& elapsed, pv_entry const* pv )
{
	scoped_lock lock( mtx );
	if( !abort ) {

		int64_t cs = elapsed.milliseconds() / 10;
		std::stringstream ss;
		ss << std::setw(2) << depth << " " << std::setw(7) << evaluation << " " << std::setw(6) << cs << " " << std::setw(10) << nodes << " " << std::setw(0) << pv_to_string( pv, p ) << std::endl;
		if( state.post ) {
			std::cout << ss.str();
		}
		else {
			std::cerr << ss.str();
		}

		best_move = pv->get_best_move();

		state.pv_move_picker_.update_pv( p, pv );
	}
}


void go( xboard_thread& thread, xboard_state& state, timestamp const& cmd_recv_time )
{
	state.last_go_time = cmd_recv_time;
	state.last_go_color = state.p.self();
	++state.moves_between_updates;

	transposition_table.init_if_needed( conf.memory );

	// Do a step
	if( conf.use_book && state.book_.is_open() && state.clock < 30 && state.started_from_root ) {
		std::vector<book_entry> moves = state.book_.get_entries( state.p, state.move_history_, true );
		if( moves.empty() ) {
			std::cerr << "Current position not in book" << std::endl;
		}
		else {
			std::cerr << "Entries from book: " << std::endl;
			std::cerr << entries_to_string( state.p, moves );

			short best = moves.front().forecast;
			int count_best = 1;
			for( std::vector<book_entry>::const_iterator it = moves.begin() + 1; it != moves.end(); ++it ) {
				if( it->forecast > -33 && it->forecast + 25 >= best && count_best < 3 ) {
					++count_best;
				}
			}

			book_entry best_move = moves[get_random_unsigned_long_long() % count_best];

			std::cout << "move " << move_to_string( best_move.m ) << std::endl;

			state.history.push_back( state.p );

			apply_move( state.p, best_move.m );
			++state.clock;
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

	move pv_move = state.pv_move_picker_.can_use_move_from_pv( state.p );
	if( !pv_move.empty() ) {
		std::cout << "move " << move_to_string( pv_move ) << std::endl;

		state.history.push_back( state.p );

		apply_move( state.p, pv_move );
		++state.clock;
		state.move_history_.push_back( pv_move );

		timestamp stop;
		state.time_remaining -= stop - state.last_go_time;
		std::cerr << "Elapsed: " << (stop - state.last_go_time).milliseconds() << " ms" << std::endl;
		return;
	}

	thread.start();
}


bool parse_setboard( xboard_state& state, xboard_thread& thread, std::string const& args, std::string& error )
{
	position new_pos;
	if( !parse_fen_noclock( args, new_pos, &error ) ) {
		return false;
	}

	bool analyze = state.mode_ == mode::analyze;

	state.reset();
	state.p = new_pos;
	state.seen.reset_root( get_zobrist_hash( state.p ) );
	state.started_from_root = false;

	if( analyze ) {
		state.mode_ = mode::analyze;
		thread.start( true );
	}

	return true;
}


void xboard( std::string line)
{
	if( uses_native_popcnt() && !cpu_has_popcnt() ) {
		std::cout << "tellusererror Your CPU does not support the POPCNT instruction, but this version of Octochess has been built to use this instruction. Please use a generic (but slower) version of Octochess for your CPU." << std::endl;
		usleep(1000);
		exit(1);
	}

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

		{
			scoped_lock l( thread.mtx );

			if( state.mode_ == mode::edit ) {
				if( !state.handle_edit_mode( cmd ) ) {
					break;
				}
				else {
					continue;
				}
			}

			// The following commands do not stop the thread.
			if( cmd == "hard" ) {
				conf.ponder = true;
				continue;
			}
			else if( cmd == "easy" ) {
				conf.ponder = false;
				continue;
			}
			else if( cmd == "." ) {
				// TODO: Implement
				std::cout << "Error (unknown command): ." << std::endl;
				continue;
			}
	#ifdef USE_STATISTICS
			else if( cmd == "~stats" ) {
				stats.print( duration() );
				stats.print_details();
				continue;
			}
	#endif
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
			std::cout << "feature memory=1" << std::endl;
			std::cout << "feature smp=1" << std::endl;

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
			state.p.c = color::white;
			state.self = color::black;
		}
		else if( cmd == "black" ) {
			state.p.c = color::black;
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
			state.fixed_move_time = duration();
		}
		else if( cmd == "go" ) {
			state.mode_ = mode::normal;
			state.self = state.p.self();
			// TODO: clocks...
			go( thread, state, cmd_recv_time );
		}
		else if( cmd == "analyze" ) {
			state.mode_ = mode::analyze;
			transposition_table.init_if_needed( conf.memory );
			thread.start( true );
		}
		else if( cmd == "exit" ) {
			state.mode_ = mode::normal;
			state.moves_between_updates = 0;
		}
		else if( cmd == "~moves" ) {
			check_map check( state.p );

			move_info moves[200];
			move_info* pm = moves;
			calculate_moves( state.p, pm, check );

			std::cout << "Possible moves:" << std::endl;
			move_info* it = &moves[0];
			for( ; it != pm; ++it ) {
				std::cout << " " << move_to_string( it->m ) << std::endl;
			}
		}
		else if( cmd == "~fen" ) {
			std::cout << position_to_fen_noclock( state.p ) << std::endl;
		}
		else if( cmd == "setboard" ) {
			std::string error;
			if( !parse_setboard( state, thread, args, error ) ) {
				std::cout << "Error (bad command): Not a valid FEN position: " << error << std::endl;
			}
		}
		else if( cmd == "~score" ) {
			std::cout << explain_eval( state.p ) << std::endl;
		}
		else if( cmd == "~hash" ) {
			std::cout << get_zobrist_hash( state.p ) << std::endl;
		}
		else if( cmd == "~see" ) {
			move m;
			std::string error;
			if( parse_move( state.p, args, m, error ) ) {
				int see_score = see( state.p, m );
				std::cout << "See score: " << see_score << std::endl;
			}
			else {
				std::cout << error << ": " << line << std::endl;
			}
		}
		else if( cmd == "st" ) {
			int64_t t;
			if( to_int<int64_t>( args, t, 1 ) ) {
				state.fixed_move_time = duration::seconds(t);
			}
			else {
				std::cout << "Error (bad command): Not a valid st command" << std::endl;
			}
		}
		else if( cmd == "memory" ) {
			unsigned int mem;
			if( to_int<unsigned int>( args, mem, 4 ) ) {
				conf.memory = mem;
			}
			else {
				std::cout << "Error (bad command): Not a valid st command" << std::endl;
			}
		}
		else if( cmd == "cores" ) {
			unsigned int cores;
			if( to_int<unsigned int>( args, cores, 1, get_cpu_count() ) ) {
				conf.thread_count = cores;
			}
			else {
				std::cout << "Error (bad command): Not a valid st command" << std::endl;
			}
		}
		else if( cmd == "edit" ) {
			state.mode_ = mode::edit;
			// We need to keep track of side to play
			state.clock = state.p.c;
			state.p.c = color::white;
		}
		else {
			move m;
			std::string error;
			if( parse_move( state.p, line, m, error ) ) {

				state.apply( m );
				if( state.mode_ == mode::normal && state.p.self() == state.self ) {
					go( thread, state, cmd_recv_time );
				}
				else if( state.mode_ == mode::analyze ) {
					thread.start( true );
				}
			}
			else {
				std::string error2;
				if( !parse_setboard( state, thread, line, error2 ) ) {
					std::cout << error << ": " << line << std::endl;
				}
			}
		}
	}
}
