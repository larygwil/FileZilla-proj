#include "assert.hpp"
#include "chess.hpp"
#include "xboard.hpp"
#include "calc.hpp"
#include "eval.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"
#include "see.hpp"
#include "selftest.hpp"
#include "state_base.hpp"
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
	analyze
};
}


struct xboard_state : public state_base
{
	xboard_state( context& ctx )
	: state_base( ctx )
		, mode_(mode::force)
		, self(color::black)
		, post(true)
		, moves_between_updates()
		, level_cmd_count()
	{
		reset();
	}

	virtual void reset( position const& p = position() )
	{
		state_base::reset( p );

		if( mode_ != mode::analyze ) {
			mode_ = mode::normal;
		}
		self = color::black;
		moves_between_updates = 0;
	}

	virtual void apply( move const& m )
	{
		state_base::apply( m );

		if( seen_.depth() >= 110 ) {
			std::cout << "1/2-1/2 (Draw)" << std::endl;
		}
	}

	virtual bool undo( unsigned int count )
	{
		if( state_base::undo( count ) ) {
			moves_between_updates = 0;
		}

		return true;
	}

	void update_comm_overhead( duration const& new_remaining, bool from_level )
	{
		duration threshold = (from_level ? duration::seconds(2) : duration::milliseconds(500)) * moves_between_updates;

		if( moves_between_updates && times_.remaining(0) != duration::infinity() && new_remaining != duration::infinity() && (times_.remaining(0) - new_remaining).absolute() < threshold ) {
			level_cmd_differences += times_.remaining(0) - new_remaining;
			level_cmd_count += moves_between_updates;

			duration comm_overhead;
			if( level_cmd_differences >= duration() ) {
				comm_overhead = level_cmd_differences / level_cmd_count;
			}
			else {
				comm_overhead = duration();
			}

			set_min( comm_overhead, duration::milliseconds( 500 ) );

			if( comm_overhead != times_.communication_overhead_ ) {
				dlog() << "Updating communication overhead from " << times_.communication_overhead_.milliseconds() << " ms to " << comm_overhead.milliseconds() << " ms " << std::endl;
				times_.communication_overhead_ = comm_overhead;
			}
		}

		moves_between_updates = 0;
	}

	mode::type mode_;
	color::type self;

	bool post;

	unsigned int moves_between_updates;

	// Level command is in seconds only. Hence we need to accumulate data before we can update the
	// communication overhead.
	duration level_cmd_differences;
	uint64_t level_cmd_count;
};


class xboard_thread : public thread, public new_best_move_callback_base
{
public:
	xboard_thread( xboard_state& s );
	virtual ~xboard_thread();

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


bool handle_edit_mode( position& p, bool frc )
{
	color::type piece_color = color::white;

	while( true ) {
		std::string line;
		std::getline( std::cin, line );

		if( !std::cin ) {
			dlog() << "EOF" << std::endl;
			return false;
		}
		if( line.empty() ) {
			continue;
		}

		std::string args;
		std::string cmd = split( line, args );

		logger::log_input( line );

		if( cmd == "c" ) {
			piece_color = other(piece_color);
		}
		else if( cmd == "#" ) {
			p.clear_bitboards();
		}
		else if( cmd == "." ) {
			p.update_derived();
			p.can_en_passant = 0;

			for( int c = 0; c < 2; ++c ) {
				uint64_t row = c ? 56 : 0;

				p.castle[c] = 0;

				if( !frc ) {
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

			if( !p.verify() ) {
				// Specs say to tell user error until next edit/new/setboard command. Since we cannot do that yet, refuse to leave edit mode.
				std::cout << "Error (invalid position): Cannot leave edit mode while the position is not valid." << std::endl;
			}
			else {
				break;
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
						p.bitboards[piece_color][pi] &= ~(1ull << sq);
					}
					if( piece > 0 ) {
						p.bitboards[piece_color][piece] |= 1ull << sq;
					}
					continue;
				}
			}
			std::cout << "Error (bad command): Not a valid command in edit mode." << std::endl;
		}
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
		// Calcualte the time available for this move
		std::pair<duration, duration> times = state.times_.update( 0, state.clock(), state.recapture() );
		
		calc_result result = cmgr_.calc( state.p(), depth_, state.times_.start(), times.first, times.second, state.clock(), state.seen(), *this, state.searchmoves_ );

		scoped_lock l( mtx );

		if( !abort ) {
			cmgr_.clear_abort();

			if( !result.best_move.empty() ) {

				xboard_output_move( state.ctx_.conf_, state.p(), result.best_move );

				state.apply( result.best_move );

				{
					score base_eval = state.p().white() ? state.p().base_eval : -state.p().base_eval;
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
			++state.moves_between_updates;

			// We applied it already.
			best_move.clear();
		}

		state.times_.update_after_move( result.used_extra_time, !abort, !abort );
	}

	if( ponder_ ) {
		dlog() << "Pondering..." << std::endl;
		cmgr_.calc( state.p(), -1, state.times_.start(), duration::infinity(), duration::infinity(), state.clock(), state.seen(), *this, state.searchmoves_ );
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
	state.times_.set_start(cmd_recv_time);

	// Do a step
	move m = state.get_book_move();
	if( !m.empty() ) {
		xboard_output_move( state.ctx_.conf_, state.p(), m );
		state.apply( m );

		state.times_.update_after_move( duration(), true, false );
		++state.moves_between_updates;
		return;
	}

	std::pair<move,move> pv_move = state.pv_move_picker_.can_use_move_from_pv( state.p() );
	if( !pv_move.first.empty() ) {
		xboard_output_move( state.ctx_.conf_, state.p(), pv_move.first );
		state.apply( pv_move.first );

		state.times_.update_after_move( duration(), true, false );
		++state.moves_between_updates;
		return;
	}

	thread.start();
}


void resume( xboard_thread& thread, xboard_state& state, timestamp const& cmd_recv_time )
{
	if( state.mode_ == mode::analyze ) {
		state.times_.set_start( cmd_recv_time );
		thread.start( true );
	}
	else if( state.mode_ == mode::normal && state.p().self() == state.self ) {
		go( thread, state, cmd_recv_time );
	}
}


bool parse_setboard( xboard_state& state, std::string const& args, std::string& error )
{
	position new_pos;
	if( !parse_fen( state.ctx_.conf_, args, new_pos, &error ) ) {
		return false;
	}

	state.reset( new_pos );
	state.self = state.p().other();

	return true;
}


void xboard( context& ctx, std::string line )
{
	if( uses_native_popcnt() && !cpu_has_popcnt() ) {
		std::cout << "tellusererror Your CPU does not support the POPCNT instruction, but this version of Octochess has been built to use this instruction. Please use a generic (but slower) version of Octochess for your CPU." << std::endl;
		millisleep(1000);
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
				if( to_int( args, d, 0, 1000 * 1000 ) ) {
					millisleep( d );
				}
				continue;
			}
			else if( cmd == "hint" ) {
				short tmp;
				move m;
				ctx.tt_.lookup( state.p().hash_, 0, 0, result::loss, result::win, tmp, m, tmp );
				if( !m.empty() ) {
					std::cout << "Hint: ";
					xboard_output_move_raw( ctx.conf_, state.p(), m );
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
				xboard_output_move( ctx.conf_, state.p(), best_move );
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
			std::cout << "feature playother=1\n";
			std::cout << "feature colors=0\n";

			std::cout << "feature done=1" << std::endl;
		}
		else if( cmd == "result" ) {
			// Ignore
		}
		else if( cmd == "new" ) {
			state.reset();
			state.times_.reset(true);
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
			state.self = color::white;
		}
		else if( cmd == "black" ) {
			state.self = color::black;
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
		else if( cmd == "playother" ) {
			state.self = state.p( ).other();
			state.mode_ = mode::normal;
		}
		else if( cmd == "time" || cmd == "otim" ) {
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
				state.times_.set_remaining( (cmd == "time") ? 0 : 1, new_remaining, state.clock() );
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

			state.times_.set_moves_to_go(control, false);
			state.times_.set_movetime( duration() );
			for( int i = 0; i < 2; ++i ) {
				state.times_.set_remaining(i, duration::minutes(minutes) + duration::seconds(seconds), state.clock() );
				state.times_.set_increment(i, duration::seconds(increment));
			}
		}
		else if( cmd == "go" ) {
			state.mode_ = mode::normal;
			state.self = state.p().self();
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
				state.times_.set_movetime(duration::seconds(t));
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
			if( to_int<unsigned int>( args, mem, 4, 1024 * 1024 * 1024 ) ) {
				ctx.conf_.memory = mem;
				ctx.tt_.init(ctx.conf_.memory);
				ctx.pawn_tt_.init( ctx.conf_.pawn_hash_table_size() );
			}
			else {
				std::cout << "Error (bad command): Not a valid memory command" << std::endl;
			}
		}
		else if( cmd == "cores" ) {
			unsigned int cores;
			if( to_int<unsigned int>( args, cores, 1, get_cpu_count() ) ) {
				ctx.conf_.thread_count = cores;
			}
			else {
				std::cout << "Error (bad command): Not a valid cores command" << std::endl;
			}
		}
		else if( cmd == "edit" ) {
			position p = state.p();
			if( !handle_edit_mode( p, state.ctx_.conf_.fischer_random ) ) {
				break;
			}
			state.reset( p );
			state.mode_ = mode::force;
		}
		else if( cmd == "usermove" ) {
			move m;
			std::string error;
			if( parse_move( state.p(), args, m, error ) ) {
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
			else if( args == "fischerrandom" || args == "fischerandom" || args == "fisherandom" || args == "fisherrandom" || args == "frc" ) {
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
			else if( parse_move( state.p(), args, m, error ) ) {
				if( exclude ) {
					if( state.searchmoves_.empty() ) {
						auto moves = calculate_moves<movegen_type::all>( state.p() );
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
#if DEVELOPMENT
		else if( cmd == "moves" ) {
			std::cout << "Possible moves:" << std::endl;
			for( auto const& m : calculate_moves<movegen_type::all>( state.p() ) ) {
				if( args == "long" ) {
					std::cout << " ";
					xboard_output_move_raw( ctx.conf_, state.p(), m );
				}
				else if( args == "full" ) {
					std::cout << " " << move_to_string( state.p(), m ) << std::endl;
				}
				else {
					std::cout << " " << move_to_san( state.p(), m ) << std::endl;
				}
			}
		}
		else if( cmd == "fen" ) {
			std::cout << position_to_fen_noclock( ctx.conf_, state.p() ) << std::endl;
		}
		else if( cmd == "score" || cmd == "eval" ) {
			std::cout << explain_eval( ctx.pawn_tt_, state.p() ) << std::endl;
		}
		else if( cmd == "hash" ) {
			std::cout << state.p().hash_ << std::endl;
		}
		else if( cmd == "see" ) {
			move m;
			std::string error;
			if( parse_move( state.p(), args, m, error ) ) {
				int see_score = see( state.p(), m );
				std::cout << "See score: " << see_score << std::endl;
			}
			else {
				std::cout << error << ": " << line << std::endl;
			}
		}
		else if( cmd == "board" ) {
			std::cout << board_to_string( state.p(), color::white );
		}
		else if( cmd == "perft" ) {
			perft( state.p() );
		}
		else if( cmd == "pv" ) {
			move pv[13];
			get_pv_from_tt( state.ctx_.tt_, pv, state.p(), 12 );
			std::cout << pv_to_string( state.ctx_.conf_, pv, state.p() ) << std::endl;
		}
#endif
		else {
			move m;
			std::string error;
			if( parse_move( state.p(), line, m, error ) ) {
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
