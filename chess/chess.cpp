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
#include "moves.hpp"
#include "util.hpp"
#include "platform.hpp"
#include "statistics.hpp"
#include "zobrist.hpp"

#include <iostream>
#include <iomanip>
#include <stdlib.h>

const int TIME_LIMIT = 30000;

std::string book_dir;

void auto_play()
{
	init_hash( conf.memory, sizeof(step_data) );
	unsigned long long start = get_time();
	position p;

	init_board(p);

	int i = 1;
	color::type c = color::white;
	move m = {0};
	int res;
	while( calc( p, c, m, res, TIME_LIMIT ) ) {
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
		apply_move( p, m, c );
		int ev = evaluate( p, color::white );
		std::cerr << "Evaluation (for white): " << ev << " centipawns" << std::endl;

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

	bool in_book = open_book( book_dir );
	if( in_book ) {
		std::cerr << "Opening book loaded" << std::endl;
	}
	unsigned long long book_index = 0;

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
				init_hash( conf.memory, sizeof(step_data) );
				hash_initialized = true;
			}
			// Do a step
			if( in_book ) {
				std::vector<move_entry> moves;
				/*book_entry entry = */
					get_entries( book_index, moves );
				if( moves.empty() ) {
					in_book = false;
				}
				else {
					short best = moves.front().forecast;
					int count_best = 0;
					std::cerr << "Entries from book: " << std::endl;
					for( std::vector<move_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
						if( it->forecast == best ) {
							++count_best;
						}
						std::cerr << move_to_string( p, c, it->get_move() ) << " " << it->forecast << std::endl;
					}

					move_entry best_move = moves[get_random_unsigned_long_long() % count_best];
					move m = best_move.get_move();
					if( !best_move.next_index ) {
						in_book = false;
						std::cerr << "Left opening book" << std::endl;
					}
					else {
						book_index = best_move.next_index;
					}

					std::cout << "move " << move_to_string( p, c, m ) << std::endl;

					apply_move( p, m, c );
					c = static_cast<color::type>( 1 - c );

					continue;
				}
			}
			move m;
			int res;
			if( calc( p, c, m, res, TIME_LIMIT ) ) {

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
			move m;
			if( parse_move( p, c, line, m ) ) {
				apply_move( p, m, c );
				c = static_cast<color::type>( 1 - c );

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
		}
	}
}


void perft( int depth, position const& p, color::type c, unsigned long long& n )
{
	if( !depth-- ) {
		++n;
		return;
	}

	move_info moves[200];
	move_info* pm = moves;

	check_map check;
	calc_check_map( p, c, check );
	calculate_moves( p, c, 0, pm, check );

	for( move_info* it = moves; it != pm; ++it ) {
		position new_pos = p;
		apply_move( new_pos, it->m, c );
		perft( depth, new_pos, static_cast<color::type>(1-c), n );
	}

}

void perft()
{
	position p;
	init_board( p );

	unsigned long long ret = 0;

	int max_depth = 6;

	unsigned long long start = get_time();
	perft( max_depth, p, color::white, ret );
	unsigned long long stop = get_time();


	std::cerr << "Moves: "     << ret << std::endl;
	std::cerr << "Took:  "     << (stop - start) << " ms" << std::endl;
	std::cerr << "Time/move: " << ((stop - start) * 1000 * 1000) / ret << " ns" << std::endl;

	if( ret != 119060324 ) {
		std::cerr << "FAIL! Expected 119060324 moves." << std::endl;
	}
	else {
		std::cerr << "PASS" << std::endl;
	}
}


int main( int argc, char const* argv[] )
{
	std::string self = argv[0];
	if( self.rfind('/') != std::string::npos ) {
		book_dir = self.substr( 0, self.rfind('/') + 1 );
	}

	console_init();

	init_random( 1234 );
	init_zobrist_tables();

	std::cerr << "  Octochess" << std::endl;
	std::cerr << "  ---------" << std::endl;
	std::cerr << std::endl;

	int i = conf.init( argc, argv );
	if( i < argc && !strcmp(argv[i], "xboard" ) ) {
		xboard();
	}
	else if( i < argc && !strcmp(argv[i], "perft" ) ) {
		perft();
	}
	else {
		auto_play();
	}
}

