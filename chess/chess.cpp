/*
Octochess
---------

Copyright (C) 2011-2012 Tim "codesquid" Kosse
http://filezilla-project.org/

Distributed under the terms and conditions of the GNU General Public License v3.

If you want to purchase a copy of Octochess under a different license, please
contact tim.kosse@filezilla-project.org for details.


*/

#include "chess.hpp"
#include "config.hpp"
#include "calc.hpp"
#include "eval.hpp"
#include "eval_values.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "util/logger.hpp"
#include "magic.hpp"
#include "moves.hpp"
#include "pawn_structure_hash_table.hpp"
#include "see.hpp"
#include "statistics.hpp"
#include "selftest.hpp"
#include "util.hpp"
#include "xboard.hpp"
#include "zobrist.hpp"

#include "uci/runner.hpp"

#if DEVELOPMENT
#include "epd.hpp"
#include "tweak.hpp"
#endif

#include <algorithm>
#include <list>
#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <sstream>

duration const TIME_LIMIT = duration::seconds(900);

void auto_play( context& ctx )
{
	ctx.tt_.init( ctx.conf_.memory );
	ctx.pawn_tt_.init( ctx.conf_.pawn_hash_table_size() );
	timestamp start;
	position p;

	unsigned int i = 1;

	seen_positions seen( p.hash_ );

	def_new_best_move_callback cb( ctx.conf_ );

	calc_manager cmgr(ctx);
	calc_result result;
	while( !(result = cmgr.calc( p, -1, timestamp(), TIME_LIMIT, TIME_LIMIT, i, seen, cb ) ).best_move.empty() ) {
		cmgr.clear_abort();
		if( p.white() ) {
			std::cout << std::setw(3) << i << ".";
		}

		std::cout << " " << move_to_string( p, result.best_move ) << std::endl;

		if( !p.white() ) {
			++i;
			std::cout << std::endl;
			if( ctx.conf_.max_moves && i > ctx.conf_.max_moves ) {
				break;
			}
		}

		if( !validate_move( p, result.best_move ) ) {
			std::cerr << std::endl << "NOT A VALID MOVE" << std::endl;
			exit(1);
		}

		bool reset_seen = false;
		pieces::type piece = p.get_piece( result.best_move );
		pieces::type captured_piece = p.get_captured_piece( result.best_move );
		if( piece == pieces::pawn || captured_piece ) {
			reset_seen = true;
		}

		apply_move( p, result.best_move );
		score base_eval = p.base_eval;
		std::cerr << "Base evaluation (for white): " << base_eval << " centipawns" << std::endl;

		if( !reset_seen ) {
			seen.push_root( p.hash_ );
		}
		else {
			seen.reset_root( p.hash_ );
		}

		if( seen.depth() > 110 ) { // Be lenient, 55 move rule is fine for us in case we don't implement this correctly.
			std::cerr << "DRAW" << std::endl;
			exit(1);
		}
	}

	timestamp stop;

	std::cerr << std::endl << "Runtime: " << (stop - start).milliseconds() << " ms " << std::endl;

#if USE_STATISTICS
	cmgr.stats().print_total();
#endif
}

int main( int argc, char const* argv[] )
{
	console_init();

	context ctx;
	std::string command = ctx.conf_.init( argc, argv );

	logger::init( ctx.conf_.logfile );

	std::cerr << ctx.conf_.program_name() << std::endl;
	std::cerr << std::endl;

	init_magic();
	pst.init();
	eval_values::init();

	init_zobrist_tables();

	bool from_stdin = false;
	while( command.empty() ) {
		if( !std::getline(std::cin, command) ) {
			std::cerr << "Could not read command" << std::endl;
			return 1;
		}
		if( !command.empty() ) {
			logger::log_input( command );
		}
		from_stdin = true;
	}

	if( command == "auto" ) {
		auto_play( ctx );
	}
	else if( command == "test" ) {
		selftest();
	}
#if DEVELOPMENT
	else if( command == "tweakgen" ) {
		generate_test_positions( ctx );
	}
	else if( command == "tweak" ) {
		tweak_evaluation( ctx );
	}
	else if( command == "sts" ) {
		run_sts( ctx );
	}
#endif
	else if( command == "xboard" ) {
		xboard( ctx, from_stdin ? "xboard" : "" );
	}
	else if( command == "uci" ) {
		run_uci( ctx, from_stdin );
	}
	else {
		xboard( ctx, command );
	}

	logger::cleanup();
}
