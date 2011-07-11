/*
Octochess
---------

Copyright (C) 2011 Tim "codesquid" Kosse
http://filezilla-project.org/

Distributed under the terms and conditions of the GNU General Public License v2+.



Ideas for optimizations:
- Factor depths into evaluation. Save evaluation different depths -> prefer the earlier position
- Transposition tables
- Iterative deepening

*/

#include "chess.hpp"
#include "calc.hpp"
#include "util.hpp"
#include "platform.hpp"
#include "statistics.hpp"

#include <iostream>
#include <iomanip>

void auto_play()
{
	unsigned long long start = get_time();
	position p;

	init_board(p);

	int i = 1;
	color::type c = color::white;
	move m = {0};
	int res = 0;
	while( calc( p, c, m, res ) ) {
		if( c == color::white ) {
			std::cout << std::setw(3) << i << ".";
		}

		std::cout << " " << move_to_string(p, c, m);

		if( c == color::black ) {
			++i;
			//check_info check = {0};
			//int i = evaluate( color::white, p, check, 0 );
			//std::cout << "  ; Evaluation: " << i << " centipawns";
			std::cout << std::endl;
		}

		if( !validate_move( p, m, c ) ) {
			std::cerr << std::endl << "NOT A VALID MOVE" << std::endl;
			exit(1);
		}
		apply_move( p, m, c );

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
			// Do a step
			move m;
			int res = 0;
			if( calc( p, c, m, res ) ) {

				std::cout << "move " << move_to_string( p, c, m ) << std::endl;

				apply_move( p, m, c );

				{
					check_info check = {0};
					int i = evaluate( c, p, check, 0 );
					std::cerr << "  ; Evaluation: " << i << " centipawns, forecast at " << res << std::endl;
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

int main( int argc, char *argv[] )
{
	console_init();

	std::cerr << "  Octochess" << std::endl;
	std::cerr << "  ---------" << std::endl;
	std::cerr << std::endl;

	srand(1234);

	if( argc >= 2 && !strcmp(argv[1], "--xboard" ) ) {
		xboard();
	}
	else {
		auto_play();
	}
}

