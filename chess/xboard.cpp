#include "assert.hpp"
#include "chess.hpp"
#include "xboard.hpp"
#include "simple_book.hpp"
#include "calc.hpp"
#include "eval.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"
#include "pv_move_picker.hpp"
#include "random.hpp"
#include "see.hpp"
#include "selftest.hpp"
#include "statistics.hpp"
#include "util/logger.hpp"
#include "util/mutex.hpp"
#include "util/string.hpp"
#include "util/thread.hpp"
#include "util.hpp"

#include <list>
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
	xboard_state( context& ctx )
		: ctx_(ctx)
		, clock()
		, book_( ctx.conf_.self_dir )
		, mode_(mode::force)
		, self(color::black)
		, time_control()
		, time_increment()
		, history()
		, post(true)
		, started_from_root()
		, internal_overhead( duration::milliseconds(50) )
		, communication_overhead( duration::milliseconds(10) )
		, last_go_color()
		, moves_between_updates()
		, level_cmd_count()
		, searchmoves_()
	{
		reset();
	}

	void reset()
	{
		p.reset();

		move_history_.clear();
		history.clear();

		clock = 1;

		seen.reset_root( p.hash_ );

		time_remaining = duration::infinity();
		bonus_time = duration();
		fixed_move_time = duration();

		if( mode_ != mode::analyze ) {
			mode_ = mode::normal;
		}
		self = color::black;

		started_from_root = true;

		searchmoves_.clear();

		moves_between_updates = 0;
	}

	void apply( move const& m )
	{
		history.push_back( p );
		bool reset_seen = false;
		pieces::type piece = p.get_piece( m );
		pieces::type captured_piece = p.get_captured_piece( m );
		if( piece == pieces::pawn || captured_piece ) {
			reset_seen = true;
		}

		apply_move( p, m );
		++clock;

		if( !reset_seen ) {
			seen.push_root( p.hash_ );
		}
		else {
			seen.reset_root( p.hash_ );
		}

		if( seen.depth() >= 110 ) {
			std::cout << "1/2-1/2 (Draw)" << std::endl;
		}

		move_history_.push_back( m );

		searchmoves_.clear();
	}

	bool undo( unsigned int count )
	{
		if( !count || count > history.size() ) {
			return false;
		}

		ASSERT( history.size() == move_history_.size() );
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

		searchmoves_.clear();

		moves_between_updates = 0;

		return true;
	}

	void update_comm_overhead( duration const& new_remaining, bool from_level )
	{
		duration threshold = (from_level ? duration::seconds(2) : duration::milliseconds(500)) * moves_between_updates;

		if( moves_between_updates && time_remaining != duration::infinity() && (time_remaining - new_remaining).absolute() < threshold ) {
			level_cmd_differences += time_remaining - new_remaining;
			level_cmd_count += moves_between_updates;

			duration comm_overhead;
			if( level_cmd_differences >= duration() ) {
				comm_overhead = level_cmd_differences / level_cmd_count;
			}
			else {
				comm_overhead = duration();
			}

			if( communication_overhead > duration::milliseconds( 500 ) ) {
				communication_overhead = duration::milliseconds( 500 );
			}

			if( comm_overhead != communication_overhead ) {
				dlog() << "Updating communication overhead from " << communication_overhead.milliseconds() << " ms to " << comm_overhead.milliseconds() << " ms " << std::endl;
				communication_overhead = comm_overhead;
			}
		}

		moves_between_updates = 0;
	}

	// Returns false if program should quit.
	bool handle_edit_mode( std::string const& cmd );

	context& ctx_;
	position p;
	int clock;
	seen_positions seen;
	simple_book book_;
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

	std::set<move> searchmoves_;

	randgen rng_;
};


class xboard_thread : public thread, public new_best_move_callback_base
{
public:
	xboard_thread( xboard_state& s );
	~xboard_thread();

	virtual void onRun();

	void start( bool just_ponder = false );

	move stop();

	mutex mtx;

	virtual void on_new_best_move( unsigned int multipv, position const& p, int depth, int selective_depth, int evaluation, uint64_t nodes, duration const& elapsed, move const* pv ) override;
	virtual bool print_only_updated() const override;

	void set_depth( int depth ) {
		depth_ = depth;
	}

	void set_multipv( unsigned int v ) {
		cmgr_.set_multipv( v );
	}

private:
	calc_manager cmgr_;
	bool abort;
	xboard_state& state;
	move best_move;
	bool ponder_;
	int depth_;
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

		for( int c = 0; c < 2; ++c ) {
			uint64_t row = c ? 56 : 0;

			p.castle[c] = 0;

			if( !ctx_.conf_.fischer_random ) {
				if( p.king_pos[c] == row + 4 ) {
					if( p.bitboards[c][bb_type::rooks] & (1ull << (row + 7)) ) {
						p.castle[c] |= 128u;
					}
					if( p.bitboards[c][bb_type::rooks] & (1ull << row)) {
						p.castle[c] |= 1u;
					}
				}
			}
			else {
				if( p.king_pos[c] > row && p.king_pos[c] < row + 8 ) {
					uint64_t q_rook = bitscan(p.bitboards[c][bb_type::rooks] & (0xffull << row));
					if( q_rook < p.king_pos[c] ) {
						p.castle[c] |= 1ull << (q_rook % 8);
					}
					uint64_t k_rook = bitscan_reverse(p.bitboards[c][bb_type::rooks] & (0xffull << row));
					if( k_rook > p.king_pos[c] ) {
						p.castle[c] |= 1ull << (k_rook % 8);
					}
				}
			}
		}
		p.hash_ = p.init_hash();

		move_history_.clear();
		history.clear();
		seen.reset_root( p.hash_ );
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
					p.bitboards[p.self()][pi] &= ~(1ull << sq);
				}
				p.bitboards[p.self()][piece] |= 1ull << sq;
				return true;
			}
		}
		std::cout << "Error (bad command): Not a valid command in edit mode." << std::endl;
	}
	return true;
}


xboard_thread::xboard_thread( xboard_state& s )
: cmgr_(s.ctx_)
, abort()
, state(s)
, ponder_()
, depth_(-1)
{
}


xboard_thread::~xboard_thread()
{
	stop();
}

	
void xboard_output_move_raw( config& conf, position const& p, move const& m )
{
	if( conf.fischer_random && m.castle() ) {
		bool kingside = (m.target() % 8) == 6;
		if( kingside ) {
			std::cout << "O-O" << std::endl;
		}
		else {
			std::cout << "O-O-O" << std::endl;
		}
	}
	else {
		std::cout << move_to_long_algebraic( conf, p, m ) << std::endl;
	}
}


void xboard_output_move( config& conf, position const& p,move const& m )
{
	std::cout << "move ";
	xboard_output_move_raw( conf, p, m );
}


void xboard_thread::onRun()
{
	if( !ponder_ ) {
		duration time_limit;
		duration deadline;
		if( !state.fixed_move_time.empty() ) {
			time_limit = duration::infinity();
			deadline = state.fixed_move_time;
		}
		else {
			if( state.bonus_time > state.time_remaining ) {
				state.bonus_time = duration();
			}

			uint64_t remaining_moves = std::max( 20, (82 - state.clock) / 2 );
			if( state.time_control ) {
				uint64_t remaining = ((state.time_control * 2) - (state.clock % (state.time_control * 2)) + 2) / 2;
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

		duration const overhead = state.internal_overhead + state.communication_overhead;
		time_limit -= overhead;
		deadline -= overhead;

		// Any less time makes no sense.
		if( time_limit < duration::milliseconds(10) ) {
			time_limit = duration::milliseconds(10);
		}
		if( deadline < duration::milliseconds(10) ) {
			deadline = duration::milliseconds(10);
		}


		calc_result result = cmgr_.calc( state.p, depth_, state.last_go_time, time_limit, deadline, state.clock, state.seen, *this, state.searchmoves_ );

		scoped_lock l( mtx );

		if( abort ) {
			return;
		}
		cmgr_.clear_abort();

		if( !result.best_move.empty() ) {

			xboard_output_move( state.ctx_.conf_, state.p, result.best_move );

			state.apply( result.best_move );

			{
				score base_eval = state.p.white() ? state.p.base_eval : -state.p.base_eval;
				dlog() << "  ; Current base evaluation: " << base_eval << " centipawns, forecast " << result.forecast << std::endl;
			}

			ponder_ = state.ctx_.conf_.ponder;
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

		dlog() << "Elapsed: " << elapsed.milliseconds() << " ms" << std::endl;
		if( time_limit.is_infinity() || elapsed < duration() ) {
			state.bonus_time.clear();
		}
		else if( time_limit > elapsed ) {
			state.bonus_time = (time_limit - elapsed) / 2;
		}
		else {
			state.bonus_time.clear();

			if( time_limit + result.used_extra_time < elapsed ) {
				duration actual_overhead = elapsed - time_limit - result.used_extra_time;
				if( actual_overhead > state.internal_overhead ) {
					dlog() << "Updating internal overhead from " << state.internal_overhead.milliseconds() << " ms to " << actual_overhead.milliseconds() << " ms " << std::endl;
					state.internal_overhead = actual_overhead;
				}
			}
		}
		if( elapsed > duration() ) {
			state.time_remaining -= elapsed;
		}
		state.time_remaining += state.time_increment;
		++state.moves_between_updates;
	}

	if( ponder_ ) {
		dlog() << "Pondering..." << std::endl;
		cmgr_.calc( state.p, -1, state.last_go_time, duration::infinity(), duration::infinity(), state.clock, state.seen, *this, state.searchmoves_ );
	}
}


void xboard_thread::start( bool just_ponder )
{
	join();
	cmgr_.clear_abort();
	abort = false;
	best_move.clear();
	ponder_ = just_ponder;

	spawn();
}


move xboard_thread::stop()
{
	cmgr_.abort();
	abort = true;
	join();
	move m = best_move;
	best_move.clear();

	return m;
}


bool xboard_thread::print_only_updated() const
{
	return true;
}


void xboard_thread::on_new_best_move( unsigned int, position const& p, int depth, int /*selective_depth*/, int evaluation, uint64_t nodes, duration const& elapsed, move const* pv )
{
	scoped_lock lock( mtx );
	if( !abort || best_move.empty() ) {

		int64_t cs = elapsed.milliseconds() / 10;
		std::stringstream ss;
		ss << std::setw(2) << depth << " " 
			<< std::setw(7) << evaluation << " "
			<< std::setw(6) << cs << " " 
			<< std::setw(10) << nodes << " " 
			<< std::setw(0) << pv_to_string( state.ctx_.conf_, pv, p ) << std::endl;
		if( state.post ) {
			std::cout << ss.str();
			std::cout.flush();
		}
		else {
			dlog() << ss.str();
		}

		best_move = *pv;

		state.pv_move_picker_.update_pv( p, pv );
	}
}


void go( xboard_thread& thread, xboard_state& state, timestamp const& cmd_recv_time )
{
	state.last_go_time = cmd_recv_time;
	state.last_go_color = state.p.self();

	// Do a step
	if( state.ctx_.conf_.use_book && state.book_.is_open() && state.clock < 30 && state.started_from_root ) {
		std::vector<simple_book_entry> moves = state.book_.get_entries( state.p );
		if( !moves.empty() ) {
			short best = moves.front().forecast;
			int count_best = 1;
			for( std::vector<simple_book_entry>::const_iterator it = moves.begin() + 1; it != moves.end(); ++it ) {
				if( it->forecast > -30 && it->forecast + 15 >= best ) {
					++count_best;
				}
			}

			simple_book_entry best_move = moves[state.rng_.get_uint64() % count_best];
			ASSERT( !best_move.m.empty() );

			xboard_output_move( state.ctx_.conf_, state.p, best_move.m );

			state.history.push_back( state.p );

			apply_move( state.p, best_move.m );
			++state.clock;
			state.move_history_.push_back( best_move.m );

			timestamp stop;
			state.time_remaining -= stop - state.last_go_time;
			state.time_remaining += state.time_increment;
			++state.moves_between_updates;
			dlog() << "Elapsed: " << (stop - state.last_go_time).milliseconds() << " ms" << std::endl;
			return;
		}
	}

	std::pair<move,move> pv_move = state.pv_move_picker_.can_use_move_from_pv( state.p );
	if( !pv_move.first.empty() ) {
		xboard_output_move( state.ctx_.conf_, state.p, pv_move.first );

		state.history.push_back( state.p );

		apply_move( state.p, pv_move.first );
		++state.clock;
		state.move_history_.push_back( pv_move.first );

		timestamp stop;
		state.time_remaining -= stop - state.last_go_time;
		state.time_remaining += state.time_increment;
		++state.moves_between_updates;
		dlog() << "Elapsed: " << (stop - state.last_go_time).milliseconds() << " ms" << std::endl;
		return;
	}

	thread.start();
}


void resume( xboard_thread& thread, xboard_state& state, timestamp const& cmd_recv_time )
{
	if( state.mode_ == mode::analyze ) {
		state.last_go_time = cmd_recv_time;
		thread.start( true );
	}
	else if( state.mode_ == mode::normal && state.p.self() == state.self ) {
		go( thread, state, cmd_recv_time );
	}
}


bool parse_setboard( xboard_state& state, std::string const& args, std::string& error )
{
	position new_pos;
	if( !parse_fen( state.ctx_.conf_, args, new_pos, &error ) ) {
		return false;
	}

	state.reset();
	state.p = new_pos;
	state.seen.reset_root( state.p.hash_ );
	state.started_from_root = false;
	state.searchmoves_.clear();
	state.self = state.p.other();

	return true;
}


void xboard( context& ctx, std::string line )
{
	if( uses_native_popcnt() && !cpu_has_popcnt() ) {
		std::cout << "tellusererror Your CPU does not support the POPCNT instruction, but this version of Octochess has been built to use this instruction. Please use a generic (but slower) version of Octochess for your CPU." << std::endl;
		usleep(1000);
		exit(1);
	}

	xboard_state state(ctx);
	xboard_thread thread( state );

	ctx.tt_.init( ctx.conf_.memory );
	ctx.pawn_tt_.init( ctx.conf_.pawn_hash_table_size() );

	if( !line.empty() ) {
		goto skip_getline;
	}

	while( true ) {
		std::getline( std::cin, line );
skip_getline:

		timestamp cmd_recv_time;

		if( !std::cin ) {
			dlog() << "EOF" << std::endl;
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
				state.searchmoves_.clear();
			}

			// The following commands do not stop the thread.
			if( cmd == "hard" ) {
				ctx.conf_.ponder = true;
				continue;
			}
			else if( cmd == "easy" ) {
				ctx.conf_.ponder = false;
				continue;
			}
			else if( cmd == "." ) {
				// TODO: Implement
				std::cout << "Error (unknown command): ." << std::endl;
				continue;
			}
			else if( cmd == "wait" ) {
				l.unlock();
				int d = 0;
				if( to_int( args, d ) ) {
					usleep( d * 1000 );
				}
				continue;
			}
			else if( cmd == "hint" ) {
				short tmp;
				move m;
				ctx.tt_.lookup( state.p.hash_, 0, 0, result::loss, result::win, tmp, m, tmp );
				if( !m.empty() ) {
					std::cout << "Hint: ";
					xboard_output_move_raw( ctx.conf_, state.p, m );
				}
				continue;
			}
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
				xboard_output_move( ctx.conf_, state.p, best_move );
				state.apply( best_move );
			}
			else {
				std::cout << "Error (command not legal now): ?" << std::endl;
			}
		}
		else if( cmd == "protover" ) {
			//std::cout << "feature ping=1" << std::endl;
			std::cout << "feature analyze=1\n";
			std::cout << "feature myname=\"" << ctx.conf_.program_name() << "\"\n";
			std::cout << "feature setboard=1\n";
			std::cout << "feature sigint=0\n";
			std::cout << "feature variants=\"normal,fischerandom\"\n";
			std::cout << "feature memory=1\n";
			std::cout << "feature smp=1\n";
			std::cout << "feature option=\"MultiPV -spin 1 1 99\"\n";
			std::cout << "feature exclude=1\n";

			std::cout << "feature done=1" << std::endl;
		}
		else if( cmd == "result" ) {
			// Ignore
		}
		else if( cmd == "new" ) {
			state.reset();
			thread.set_depth( -1 );
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
		}
		else if( cmd == "remove" ) {
			if( !state.undo(2) ) {
				std::cout << "Error (command not legal now): remove" << std::endl;
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
				duration new_remaining = duration::milliseconds( t * 10 );
				state.update_comm_overhead( new_remaining, false );
				state.time_remaining = new_remaining;
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

			state.update_comm_overhead( duration::minutes(minutes) + duration::seconds(seconds), true );

			state.time_control = control;
			state.time_remaining = duration::minutes(minutes) + duration::seconds(seconds);
			state.time_increment = duration::seconds(increment);
			state.fixed_move_time = duration();
		}
		else if( cmd == "go" ) {
			state.mode_ = mode::normal;
			state.self = state.p.self();
		}
		else if( cmd == "analyze" ) {
			state.mode_ = mode::analyze;
		}
		else if( cmd == "exit" ) {
			state.mode_ = mode::normal;
			state.moves_between_updates = 0;
		}
		else if( cmd == "setboard" ) {
			std::string error;
			if( !parse_setboard( state, args, error ) ) {
				std::cout << "Error (bad command): Not a valid FEN position: " << error << std::endl;
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
		else if( cmd == "sd" ) {
			short d;
			if( to_int<short>( args, d, 1, MAX_DEPTH ) ) {
				thread.set_depth( d );
			}
			else {
				std::cout << "Error (bad command): Not a valid sd command" << std::endl;
			}
		}
		else if( cmd == "memory" ) {
			unsigned int mem;
			if( to_int<unsigned int>( args, mem, 4 ) ) {
				ctx.conf_.memory = mem;
				ctx.tt_.init(ctx.conf_.memory);
				ctx.pawn_tt_.init( ctx.conf_.pawn_hash_table_size() );
			}
			else {
				std::cout << "Error (bad command): Not a valid st command" << std::endl;
			}
		}
		else if( cmd == "cores" ) {
			unsigned int cores;
			if( to_int<unsigned int>( args, cores, 1, get_cpu_count() ) ) {
				ctx.conf_.thread_count = cores;
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
		else if( cmd == "usermove" ) {
			move m;
			std::string error;
			if( parse_move( state.p, args, m, error ) ) {
				state.apply( m );
			}
			else {
				std::cout << "Error (bad command): Not a valid move: " << error << std::endl;
			}
		}
		else if( cmd == "variant" ) {
			if( args == "normal" ) {
				ctx.conf_.fischer_random = false;
			}
			else if( args == "fischerandom" || args == "fisherrandom" || args == "frc" ) {
				ctx.conf_.fischer_random = true;
			}
			else {
				std::cout << "Error (bad command): Not a valid variant" << std::endl;
			}
		}
		else if( cmd == "option" ) {
			std::string value;
			std::string name = split( args, value, '=' );

			if( name == "MultiPV" ) {
				unsigned int v;
				if( !to_int<unsigned int>( value, v, 1, 99 ) ) {
					std::cout << "Error (bad command): Not a valid value" << std::endl;
				}
				else {
					thread.set_multipv( v );
				}
			}
			else {
				std::cout << "Error (bad command): Not a known option" << std::endl;
			}
		}
		else if( cmd == "exclude" || cmd == "include" ) {
			bool exclude = cmd == "exclude";

			move m;
			std::string error;

			if( args == "all" ) {
				state.searchmoves_.clear();
				if( exclude ) {
					state.searchmoves_.insert( move() );
				}
			}
			else if( parse_move( state.p, args, m, error ) ) {
				if( exclude ) {
					if( state.searchmoves_.empty() ) {
						auto moves = calculate_moves<movegen_type::all>( state.p );
						state.searchmoves_.insert( moves.begin(), moves.end() );
					}
					state.searchmoves_.erase( m );
					if( state.searchmoves_.empty() ) {
						state.searchmoves_.insert( move() );
					}
				}
				else {
					state.searchmoves_.insert( m );
					state.searchmoves_.erase( move() );
				}
			}
			else {
				std::cout << "Error (bad command): Not a valid move" << std::endl;
			}

			ASSERT( state.searchmoves_.find( move() ) == state.searchmoves_.end() || state.searchmoves_.size() == 1 );
		}
		// Octochess-specific commands mainly for testing and debugging
		else if( cmd == "moves" ) {
			std::cout << "Possible moves:" << std::endl;
			for( auto m : calculate_moves<movegen_type::all>( state.p ) ) {
				if( args == "long" ) {
					std::cout << " ";
					xboard_output_move_raw( ctx.conf_, state.p, m );
				}
				else if( args == "full" ) {
					std::cout << " " << move_to_string( state.p, m ) << std::endl;
				}
				else {
					std::cout << " " << move_to_san( state.p, m ) << std::endl;
				}
			}
		}
		else if( cmd == "fen" ) {
			std::cout << position_to_fen_noclock( ctx.conf_, state.p ) << std::endl;
		}
		else if( cmd == "score" || cmd == "eval" ) {
			std::cout << explain_eval( ctx.pawn_tt_, state.p ) << std::endl;
		}
		else if( cmd == "hash" ) {
			std::cout << state.p.hash_ << std::endl;
		}
		else if( cmd == "see" ) {
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
		else if( cmd == "board" ) {
			std::cout << board_to_string( state.p, color::white );
		}
		else if( cmd == "perft" ) {
			perft( state.p );
		}
		else {
			move m;
			std::string error;
			if( parse_move( state.p, line, m, error ) ) {
				state.apply( m );
			}
			else {
				// Octochess-specific extension: Raw fen without command is equivalent to setboard.
				std::string error2;
				if( !parse_setboard( state, line, error2 ) ) {
					std::cout << error << ": " << line << std::endl;
				}
			}
		}

		resume( thread, state, cmd_recv_time );
	}
}
