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
#include "epd.hpp"
#include "eval.hpp"
#include "eval_values.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "util/logger.hpp"
#include "magic.hpp"
#include "moves.hpp"
#include "pawn_structure_hash_table.hpp"
#include "random.hpp"
#include "see.hpp"
#include "statistics.hpp"
#include "selftest.hpp"
#include "tweak.hpp"
#include "util.hpp"
#include "xboard.hpp"
#include "zobrist.hpp"

#include "uci/runner.hpp"

#include <algorithm>
#include <list>
#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <sstream>

duration const TIME_LIMIT = duration::seconds(90);

void auto_play()
{
	transposition_table.init( conf.memory );
	pawn_hash_table.init( conf.pawn_hash_table_size() );
	timestamp start;
	position p;

	unsigned int i = 1;

	seen_positions seen( p.hash_ );

	calc_manager cmgr;
	calc_result result;
	while( !(result = cmgr.calc( p, -1, TIME_LIMIT, TIME_LIMIT, i, seen ) ).best_move.empty() ) {
		cmgr.clear_abort();
		if( p.white() ) {
			std::cout << std::setw(3) << i << ".";
		}

		std::cout << " " << move_to_string( p, result.best_move ) << std::endl;

		if( !p.white() ) {
			++i;
			std::cout << std::endl;
			if( conf.max_moves && i > conf.max_moves ) {
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

	std::string command = conf.init( argc, argv );

	logger::init( conf.logfile );

	std::cerr << conf.program_name() << std::endl;
	std::cerr << std::endl;

	init_magic();
	init_pst();
	eval_values::init();

	if( conf.random_seed != -1 ) {
		init_random( conf.random_seed );
	}
	else {
		uint64_t seed = get_time();
		init_random(seed);
		std::cerr << "Random seed is " << seed << std::endl;
	}

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
		auto_play();
	}
	else if( command == "perft" ) {
		pawn_hash_table.init( conf.pawn_hash_table_size() );
		perft<false>();
	}
	else if( command == "test" ) {
		selftest();
	}
	else if( command == "tweakgen" ) {
		generate_test_positions();
	}
	else if( command == "tweak" ) {
		tweak_evaluation();
	}
	else if( command == "xboard" ) {
		xboard( from_stdin ? "xboard" : "" );
	}
	else if( command == "uci" ) {
		run_uci( from_stdin );
	}
	else if( command == "sts" ) {
		run_sts();
	}
	else {
		xboard( command );
	}

	logger::cleanup();
}
