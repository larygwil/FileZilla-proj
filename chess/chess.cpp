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
#include "pawn_structure_hash_table.hpp"
#include "platform.hpp"
#include "statistics.hpp"
#include "zobrist.hpp"

#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <sstream>

const int TIME_LIMIT = 90000; //30000;

const int PAWN_HASH_TABLE_SIZE = 5;

std::string book_dir;

void auto_play()
{
	transposition_table.init( conf.memory );
	pawn_hash_table.init( PAWN_HASH_TABLE_SIZE );
	unsigned long long start = get_time();
	position p;

	init_board(p);

	unsigned int i = 1;
	color::type c = color::white;
	move m;
	int res;

	seen_positions seen;
	seen.root_position = 0;
	seen.pos[0] = get_zobrist_hash( p, c );

	while( calc( p, c, m, res, TIME_LIMIT * timer_precision() / 1000, i, seen ) ) {
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

		bool reset_seen = false;
		int pi = p.board[m.source_col][m.source_row] & 0x0f;
		if( (pi >= pieces::pawn1 && pi <= pieces::pawn8 && !p.pieces[c][pi].special) || p.board[m.target_col][m.target_row] != pieces::nil ) {
			reset_seen = true;
		}

		bool captured;
		apply_move( p, m, c, captured );
		int ev = evaluate_fast( p, color::white );
		std::cerr << "Evaluation (for white): " << ev << " centipawns" << std::endl;

		c = static_cast<color::type>(1-c);

		if( !reset_seen ) {
			seen.pos[++seen.root_position] = get_zobrist_hash( p, c );
		}
		else {
			seen.root_position = 0;
			seen.pos[0] = get_zobrist_hash( p, c );
		}

		if( seen.root_position > 110 ) { // Be lenient, 55 move rule is fine for us in case we don't implement this correctly.
			std::cerr << "DRAW" << std::endl;
			exit(1);
		}
	}

	unsigned long long stop = get_time();

	std::cerr << std::endl << "Runtime: " << (stop - start) * 1000 / timer_precision() << " ms " << std::endl;

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

	int clock = 1;

	seen_positions seen;
	seen.root_position = 0;
	seen.pos[0] = get_zobrist_hash( p, c );

	unsigned long long time_remaining = conf.time_limit * timer_precision() / 1000;

	pawn_hash_table.init( PAWN_HASH_TABLE_SIZE );

	while( true ) {
		std::getline( std::cin, line );
		if( !std::cin ) {
			std::cerr << "EOF" << std::endl;
			break;
		}
		if( line == "force" ) {
			// Ignore
		}
		else if( line.substr( 0, 6 ) == "level " ) {
			line = line.substr( 6 );
			std::stringstream ss;
			ss.flags(std::stringstream::skipws);
			ss.str(line);

			unsigned int unused;
			ss >> unused;

			std::string time;
			ss >> time;

			std::string unused2;
			ss >> unused;

			if( !ss ) {
				std::cout << "Not a valid level command" << std::endl;
				continue;
			}

			std::stringstream ss2;
			ss2.str(time);

			unsigned int minutes;
			unsigned int seconds = 0;

			ss2 >> minutes;
			if( !ss2 ) {
				std::cout << "Not a valid level command" << std::endl;
				continue;
			}

			char ch;
			if( ss2 >> ch ) {
				if( ch == ':' ) {
					ss2 >> seconds;
					if( !ss2 ) {
						std::cout << "Not a valid level command" << std::endl;
						continue;
					}
				}
				else {
					std::cout << "Not a valid level command" << std::endl;
					continue;
				}
			}

			time_remaining = minutes * 60 + seconds;
			time_remaining *= timer_precision();
		}
		else if( line == "go" ) {
			unsigned long long start = get_time();
			if( !hash_initialized ) {
				transposition_table.init( conf.memory );
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
						if( it->forecast + 25 >= best ) {
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

					bool captured;
					apply_move( p, m, c, captured );
					++clock;
					c = static_cast<color::type>( 1 - c );

					unsigned long long stop = get_time();
					time_remaining -= stop - start;
					continue;
				}
			}

			int remaining_moves = std::max( 10, (71 - clock) / 2 );
			unsigned long long time_limit = time_remaining / remaining_moves;
			unsigned long long overhead_compensation = 50 * timer_precision() / 1000;
			if( time_limit > overhead_compensation ) {
				time_limit -= overhead_compensation;
			}

			move m;
			int res;
			if( calc( p, c, m, res, time_limit, clock, seen ) ) {

				std::cout << "move " << move_to_string( p, c, m ) << std::endl;

				bool reset_seen = false;
				int pi = p.board[m.source_col][m.source_row] & 0x0f;
				if( (pi >= pieces::pawn1 && pi <= pieces::pawn8 && !p.pieces[c][pi].special) || p.board[m.target_col][m.target_row] != pieces::nil ) {
					reset_seen = true;
				}

				bool captured;
				apply_move( p, m, c, captured );
				++clock;

				{
					int i = evaluate_fast( p, c );
					std::cerr << "  ; Current evaluation " << i << " centipawns, forecast " << res << std::endl;
				}

				c = static_cast<color::type>( 1 - c );

				if( !reset_seen ) {
					seen.pos[++seen.root_position] = get_zobrist_hash( p, c );
				}
				else {
					seen.root_position = 0;
					seen.pos[0] = get_zobrist_hash( p, c );
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
			unsigned long long stop = get_time();
			time_remaining -= stop - start;
		}
		else {
			move m;
			if( parse_move( p, c, line, m ) ) {

				bool reset_seen = false;
				int pi = p.board[m.source_col][m.source_row] & 0x0f;
				if( (pi >= pieces::pawn1 && pi <= pieces::pawn8 && !p.pieces[c][pi].special) || p.board[m.target_col][m.target_row] != pieces::nil ) {
					reset_seen = true;
				}

				bool captured;
				apply_move( p, m, c, captured );
				++clock;
				c = static_cast<color::type>( 1 - c );

				if( !reset_seen ) {
					seen.pos[++seen.root_position] = get_zobrist_hash( p, c );
				}
				else {
					seen.root_position = 0;
					seen.pos[0] = get_zobrist_hash( p, c );
				}

				if( seen.root_position >= 110 ) {
					std::cout << "1/2-1/2 (Draw)" << std::endl;
				}

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
		bool captured;
		apply_move( new_pos, *it, c, captured );
		perft( depth, new_pos, static_cast<color::type>(1-c), n );
	}

}

void perft()
{
	pawn_hash_table.init( PAWN_HASH_TABLE_SIZE );

	position p;
	init_board( p );

	unsigned long long ret = 0;

	int max_depth = 6;

	unsigned long long start = get_time();
	perft( max_depth, p, color::white, ret );
	unsigned long long stop = get_time();


	std::cerr << "Moves: "     << ret << std::endl;
	std::cerr << "Took:  "     << (stop - start) * 1000 / timer_precision() << " ms" << std::endl;
	std::cerr << "Time/move: " << ((stop - start) * 1000 * 1000 * 1000) / ret / timer_precision() << " ns" << std::endl;

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

	std::cerr << "  Octochess" << std::endl;
	std::cerr << "  ---------" << std::endl;
	std::cerr << std::endl;

	int i = conf.init( argc, argv );

	if( conf.random_seed != -1 ) {
		init_random( conf.random_seed );
	}
	else {
		unsigned long long seed = get_time();
		init_random(seed);
		std::cerr << "Random seed is " << seed << std::endl;
	}
	init_zobrist_tables();

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
