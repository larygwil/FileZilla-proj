#include "calc.hpp"
#include "epd.hpp"
#include "config.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"
#include "util/logger.hpp"
#include "util/string.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

duration const d = duration::milliseconds(1000);

extern volatile bool do_abort;

std::vector<epd> parse_epd( std::string const& fn )
{
	std::ifstream in( fn );
	if( !in.is_open() ) {
		std::cerr << in << " not found" << std::endl;
		abort();
	}

	return parse_epd( in );
}

std::vector<epd> parse_epd( std::istream& in )
{
	std::vector<epd> ret;
	std::string line;
	while( std::getline( in, line ) != 0 ) {

		std::vector<std::string> tokens = tokenize( line, " ;", '"' );
		if( tokens.size() < 4 ) {
			std::cerr << "Could not parse " << line << std::endl;
			abort();
		}

		std::string fen;
		for( int i = 0; i < 4; ++i ) {
			if( !fen.empty() ) {
				fen += " ";
			}
			fen += tokens[i];
		}

		epd e;
		if( !parse_fen( fen, e.p ) ) {
			std::cerr << "Could not parse " << fen << std::endl;
			abort();
		}

		for( std::size_t i = 4; i < tokens.size(); ++i ) {
			std::string op = tokens[i++];
			if( i == tokens.size() ) {
				abort();
			}
			e.operations[op] = tokens[i];
		}

		auto it = e.operations.find("bm");
		if( it != e.operations.end() ) {
			std::string error;
			if( !parse_move( e.p, it->second, e.best_move, error ) ) {
				abort();
			}
			e.operations.erase(it);
		}

		ret.push_back(e);
	}

	return ret;
}

int run_sts( epd const& e, int& match, int& sum )
{
	auto it = e.operations.find("c0");
	if( it != e.operations.end() && !it->second.empty()) {
		std::vector<std::string> tokens = tokenize( it->second, " ,=" );
		if( tokens.size() % 2 ) {
			abort();
		}

		std::map<move, unsigned int> scores;

		for( std::size_t i = 0; i < tokens.size(); i += 2 ) {
			unsigned int move_score;
			if( !to_int( tokens[i+1], move_score ) ) {
				abort();
			}

			move m;
			std::string error;
			if( !parse_move( e.p, tokens[i], m, error ) ) {
				continue;
			}
			scores[m] = move_score;
		}

		transposition_table.init(conf.memory);
		pawn_hash_table.init( conf.pawn_hash_table_size() );

		calc_manager c;
		seen_positions seen( get_zobrist_hash( e.p ) );
		do_abort = false;
		calc_result r = c.calc( e.p, conf.max_search_depth(), d, d, 0, seen, 0, null_new_best_move_cb );
		if( scores.find(r.best_move) != scores.end() ) {
			sum += scores[r.best_move];
		}

		if( r.best_move == e.best_move ) {
			++match;
		}
	}

	return sum;
}

void run_sts()
{
	logger::show_debug( false );

	std::vector<int> matches;
	std::vector<int> scores;
	for( int i = 1; i <= 14; ++i ) {
		std::ostringstream ss;
		ss << conf.self_dir << "sts/STS" << i << ".epd";

		std::vector<epd> epds = parse_epd( ss.str() );

		int match = 0;
		int score = 0;

		for( auto e : epds ) {
			run_sts( e, match, score );
		}

		std::cerr << i << " " << match << " " << score << std::endl;

		matches.push_back(match);
		scores.push_back(score);
	}

	std::cerr << "\n\n\n";
	int match_sum = 0;
	int score_sum = 0;
	for( std::size_t i = 0; i < matches.size(); ++i ) {
		match_sum += matches[i];
		score_sum += scores[i];
		std::cerr << i << " " << matches[i] << " " << scores[i] << std::endl;
	}
	std::cerr << "Sum " << match_sum << " " << score_sum << std::endl;
}
