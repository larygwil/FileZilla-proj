/*
Octochess
---------

Copyright (C) 2011-2012 Tim "codesquid" Kosse
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
#include "eval_values.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "logger.hpp"
#include "magic.hpp"
#include "moves.hpp"
#include "pawn_structure_hash_table.hpp"
#include "platform.hpp"
#include "random.hpp"
#include "see.hpp"
#include "statistics.hpp"
#include "selftest.hpp"
#include "time.hpp"
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
	if( conf.depth == -1 ) {
		conf.depth = 8;
	}
	transposition_table.init( conf.memory );
	pawn_hash_table.init( conf.pawn_hash_table_size );
	timestamp start;
	position p;

	init_board(p);

	unsigned int i = 1;
	color::type c = color::white;
	move m;
	int res = 0;

	seen_positions seen( get_zobrist_hash( p ) );

	short last_mate = 0;

	calc_manager cmgr;
	while( cmgr.calc( p, c, m, res, TIME_LIMIT, i, seen, last_mate ) ) {
		if( c == color::white ) {
			std::cout << std::setw(3) << i << ".";
		}

		if( res > result::win_threshold ) {
			last_mate = res;
		}
		std::cout << " " << move_to_string( m ) << std::endl;

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
		score base_eval = p.base_eval;
		std::cerr << "Base evaluation (for white): " << base_eval << " centipawns" << std::endl;

		c = static_cast<color::type>(1-c);

		if( !reset_seen ) {
			seen.push_root( get_zobrist_hash( p ) );
		}
		else {
			seen.reset_root( get_zobrist_hash( p ) );
		}

		if( seen.depth() > 110 ) { // Be lenient, 55 move rule is fine for us in case we don't implement this correctly.
			std::cerr << "DRAW" << std::endl;
			exit(1);
		}
	}

	timestamp stop;

	std::cerr << std::endl << "Runtime: " << (stop - start).milliseconds() << " ms " << std::endl;

#ifdef USE_STATISTICS
	stats.print_total();
#endif
}

int main( int argc, char const* argv[] )
{
	console_init();

	std::string const command = conf.init( argc, argv );

	logger::init( conf.logfile );

	std::cerr << "  Octochess" << std::endl;
	std::cerr << "  ---------" << std::endl;
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

	if( command == "auto" ) {
		auto_play();
	}
	else if( command == "perft" ) {
		pawn_hash_table.init( conf.pawn_hash_table_size );
		perft();
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
		xboard( "" );
	}
	else if( command == "uci" ) {
		run_uci( false );
	}
	else if( command.empty() ) {
		std::string line;
		if( !line.empty() ) {
			logger::log_input( line );
		}
		if( !std::getline(std::cin, line) ) {
			std::cerr << "Could not read command" << std::endl;
		}
		else if( line == "uci" ) {
			run_uci( true );
		}
		else {
			xboard( line );
		}
	}
	else {
		std::cerr << "Unknown command: " << command << std::endl;
		exit(1);
	}

	logger::cleanup();
}
