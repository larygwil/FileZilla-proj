/*
Octochess
---------

Copyright (C) 2011 Tim "codesquid" Kosse
http://filezilla-project.org/

Distributed under the terms and conditions of the GNU General Public License v3.

If you want to purchase a copy of Octochess under a different license, please
contact tim.kosse@filezilla-project.org for details.


*/

#include "chess.hpp"
#include "calc.hpp"
#include "eval.hpp"
#include "hash.hpp"
#include "moves.hpp"
#include "util.hpp"
#include "platform.hpp"
#include "statistics.hpp"
#include "zobrist.hpp"

#include <iostream>
#include <iomanip>
#include <stdlib.h>

void auto_play( int argc, char const* argv[] )
{
	int max_moves = 0;

	{
		int i = 1;
		while( i < argc ) {
			if( !strcmp(argv[i], "--moves" ) ) {
				if( ++i == argc ) {
					std::cerr << "Missing argument to --moves" << std::endl;
					exit(1);
				}
				max_moves = atoi(argv[i++]);
			}
			else {
				std::cerr << "Unknown argument" << std::endl;
				exit(1);
			}
		}
	}

	init_hash( 2048+1024, sizeof(step_data) );
	unsigned long long start = get_time();
	position p;

	init_board(p);

	int i = 1;
	color::type c = color::white;
	move m = {0};
	int res;
	while( calc( p, c, m, res ) ) {
		if( c == color::white ) {
			std::cout << std::setw(3) << i << ".";
		}

		std::cout << " " << move_to_string(p, c, m);

		if( c == color::black ) {
			++i;
			std::cout << std::endl;
			if( max_moves && i > max_moves ) {
				break;
			}
		}

		if( !validate_move( p, m, c ) ) {
			std::cerr << std::endl << "NOT A VALID MOVE" << std::endl;
			exit(1);
		}
		apply_move( p, m, c );
		int ev = evaluate( p, color::white );
		std::cout << "  ; Evaluation (for white): " << ev << " centipawns";

		c = static_cast<color::type>(1-c);
	}

	unsigned long long stop = get_time();

	std::cerr << std::endl << "Runtime: " << stop - start << " ms " << std::endl;

#ifdef USE_STATISTICS
	print_stats( start, stop );
#endif
}

void xboard()
{
	bool hash_initialized = false;

	position p;

	init_board(p);

	std::string line;
	std::getline( std::cin, line );
	if( line != "xboard" ) {
		std::cerr << "First command needs to be xboard!" << std::endl;
		exit(1);
	}
	std::cout << std::endl;

	color::type c = color::white;

	while( true ) {
		std::getline( std::cin, line );
		if( line == "force" ) {
			// Ignore
		}
		else if( line == "go" ) {
			if( !hash_initialized ) {
				init_hash( 2048+1024, sizeof(step_data) );
				hash_initialized = true;
			}
			// Do a step
			move m;
			int res;
			if( calc( p, c, m, res ) ) {

				std::cout << "move " << move_to_string( p, c, m ) << std::endl;

				apply_move( p, m, c );

				{
					int i = evaluate( p, c );
					std::cerr << "  ; Current evaluation " << i << " centipawns, forecast " << res << std::endl;
				}

				c = static_cast<color::type>( 1 - c );
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
		}
		else {
			parse_move( p, c, line );
		}
	}
}

int main( int argc, char const* argv[] )
{
	console_init();

	init_random( 1234 );
	init_zobrist_tables();

	std::cerr << "  Octochess" << std::endl;
	std::cerr << "  ---------" << std::endl;
	std::cerr << std::endl;

	if( argc >= 2 && !strcmp(argv[1], "--xboard" ) ) {
		xboard();
	}
	else {
		auto_play( argc, argv );
	}
}

