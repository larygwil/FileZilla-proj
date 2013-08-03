#if DEVELOPMENT
#include "calc.hpp"
#include "epd.hpp"
#include "config.hpp"
#include "fen.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"
#include "util/logger.hpp"
#include "util/string.hpp"
#include "util.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

duration const d = duration::milliseconds(1000);

std::vector<epd> parse_epd( config const& conf, std::string const& fn )
{
	std::ifstream in( fn );
	if( !in.is_open() ) {
		std::cerr << fn << " not found" << std::endl;
		abort();
	}

	return parse_epd( conf, in );
}

std::vector<epd> parse_epd( config const& conf, std::istream& in )
{
	std::vector<epd> ret;
	std::string line;
	while( std::getline( in, line ).good() ) {

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
		if( !parse_fen( conf, fen, e.p ) ) {
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

int run_sts( context& ctx, epd const& e, int& match, int& sum )
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

		ctx.tt_.init( ctx.conf_.memory, true );
		ctx.pawn_tt_.init( ctx.conf_.pawn_hash_table_size(), true );

		calc_manager c(ctx);
		seen_positions seen( e.p.hash_ );
		calc_result r = c.calc( e.p, ctx.conf_.max_search_depth(), timestamp(), duration::infinity(), d, 0, seen, null_new_best_move_cb );
		if( scores.find(r.best_move) != scores.end() ) {
			sum += scores[r.best_move];
		}

		if( r.best_move == e.best_move ) {
			++match;
		}
	}

	return sum;
}

void run_sts( context& ctx )
{
	logger::show_debug( false );

	std::vector<int> matches;
	std::vector<int> scores;
	for( int i = 1; i <= 14; ++i ) {
		std::ostringstream ss;
		ss << ctx.conf_.self_dir << "sts/STS" << i << ".epd";

		std::vector<epd> epds = parse_epd( ctx.conf_, ss.str() );

		int match = 0;
		int score = 0;

		for( auto e : epds ) {
			run_sts( ctx, e, match, score );
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
#endif