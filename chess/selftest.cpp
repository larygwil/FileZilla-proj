#include "calc.hpp"
#include "chess.hpp"
#include "fen.hpp"
#include "moves.hpp"
#include "selftest.hpp"
#include "util.hpp"
#include "pawn_structure_hash_table.hpp"
#include "config.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <vector>

extern const int PAWN_HASH_TABLE_SIZE = 10;

struct perft_ctx {
	perft_ctx()
		: move_ptr(moves)
	{
	}

	move_info moves[200 * (MAX_DEPTH) ];
	move_info* move_ptr;

	killer_moves killers;
};


void perft( perft_ctx& ctx, int depth, position const& p, color::type c, unsigned long long& n )
{
	if( conf.depth == -1 ) {
		conf.depth = 8;
	}

	move_info* moves = ctx.move_ptr;

	check_map check;
	calc_check_map( p, c, check );
	calculate_moves( p, c, ctx.move_ptr, check );

	if( !--depth ) {
		n += ctx.move_ptr - moves;
		ctx.move_ptr = moves;
		return;
	}

	for( move_info* it = moves; it != ctx.move_ptr; ++it ) {
		position new_pos = p;
		apply_move( new_pos, it->m, c );
		perft( ctx, depth, new_pos, static_cast<color::type>(1-c), n );
	}
	ctx.move_ptr = moves;
}

bool perft( std::size_t max_depth )
{
	unsigned long long const perft_results[] = {
		20ull,
		400ull,
		8902ull,
		197281ull,
		4865609ull,
		119060324ull,
		3195901860ull,
		84998978956ull
	};

	/*
		alternate:r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -
		6
		264
		9467
		422333
		15833292
		706045033
	*/

	perft_ctx ctx;
	for( unsigned int i = 0; i < (std::min)(max_depth, sizeof(perft_results)/sizeof(unsigned long long)); ++i ) {
		ctx.move_ptr = ctx.moves;

		std::cerr << "Calculating number of possible moves in " << (i + 1) << " plies:" << std::endl;

		position p;
		init_board( p );
		color::type c = color::white;

		unsigned long long ret = 0;

		int max_depth = i + 1;

		unsigned long long start = get_time();
		perft( ctx, max_depth, p, c, ret );
		unsigned long long stop = get_time();


		std::cerr << "Moves: "     << ret << std::endl;
		std::cerr << "Took:  "     << (stop - start) * 1000 / timer_precision() << " ms" << std::endl;
		if( ret ) {
			// 64bit integers are getting too small too fast.
			unsigned long long factor = 1000ull * 1000 * 1000 * 1000;
			unsigned long long divisor;
			if( timer_precision() > factor ) {
				divisor = timer_precision() / factor;
				factor = 1;
			}
			else {
				factor /= timer_precision();
				divisor = 1;
			}
			unsigned long long picoseconds = ((stop - start) * factor) / ret / divisor;
			std::stringstream ss;
			ss << "Time/move: " << picoseconds / 1000 << "." << std::setw(1) << std::setfill('0') << (picoseconds / 100) % 10 << " ns" << std::endl;
			std::cerr << ss.str();
		}

		if( ret != perft_results[i] ) {
			std::cerr << "FAIL! Expected " << perft_results[i] << " moves." << std::endl;
			return false;
		}
		else {
			std::cerr << "PASS" << std::endl;
		}
		std::cerr << std::endl;
	}


	return true;
}


namespace {

static bool test_position( std::string const& fen, std::string const& ref_moves )
{
	position p;
	color::type c;
	if( !parse_fen_noclock( fen, p, c ) ) {
		std::cerr << "Could not parse fen: " << fen;
		return false;
	}

	move_info moves[200];
	move_info* pm = moves;
	check_map check;
	calc_check_map( p, c, check );
	calculate_moves( p, c, pm, check );

	std::vector<std::string> ms;
	for( move_info* it = moves; it != pm; ++it ) {
		ms.push_back( move_to_string( it->m ) );
	}
	std::sort( ms.begin(), ms.end() );

	std::string s;
	for( std::size_t i = 0; i < ms.size(); ++i ) {
		if( i ) {
			s += " ";
		}
		std::size_t pos;
		while( (pos = ms[i].find( ' ' ) ) != std::string::npos ) {
			ms[i].erase( pos, 1 );
		}
		s += ms[i];
	}

	if( ref_moves != s ) {
		std::cerr << "Move mismatch!" << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		std::cerr << "Reference: " << ref_moves << std::endl;
		std::cerr << "Actual:    " << s << std::endl;

		return false;
	}

	return true;
}


static bool test_positions()
{
	if( !test_position( "4R3/p1pp1p1p/b1n1rn2/1p2p1pP/1B1QPBr1/qPPb2PN/Pk1PNP2/R3K3 w Q g6",
						"c3-c4 f2-f3 h5-h6 h5xg6 Bb4-a5 Bb4-c5 Bb4-d6 Bb4-e7 Bb4-f8 Bb4xa3 Bf4-e3 Bf4xe5 Bf4xg5 Ke1-d1 Ke1-f1 Ne2-c1 Ne2-g1 Nh3-g1 Nh3xg5 Qd4-b6 Qd4-c4 Qd4-c5 Qd4-d5 Qd4-d6 Qd4-e3 Qd4xa7 Qd4xd3 Qd4xd7 Qd4xe5 Ra1-b1 Ra1-c1 Ra1-d1 Re8-a8 Re8-b8 Re8-c8 Re8-d8 Re8-e7 Re8-f8 Re8-g8 Re8-h8 Re8xe6" ) )
	{
		return false;
	}

	if( !test_position( "r3k2r/8/2p5/1Q6/1q6/2P5/8/R3K2R w KqQk -",
						"O-O O-O-O c3xb4 Ke1-d1 Ke1-d2 Ke1-e2 Ke1-f1 Ke1-f2 Qb5-a4 Qb5-a5 Qb5-a6 Qb5-b6 Qb5-b7 Qb5-b8 Qb5-c4 Qb5-c5 Qb5-d3 Qb5-d5 Qb5-e2 Qb5-e5 Qb5-f1 Qb5-f5 Qb5-g5 Qb5-h5 Qb5xb4 Qb5xc6 Ra1-a2 Ra1-a3 Ra1-a4 Ra1-a5 Ra1-a6 Ra1-a7 Ra1-b1 Ra1-c1 Ra1-d1 Ra1xa8 Rh1-f1 Rh1-g1 Rh1-h2 Rh1-h3 Rh1-h4 Rh1-h5 Rh1-h6 Rh1-h7 Rh1xh8" ) )
	{
		return false;
	}

	return true;
}


static bool test_zobrist()
{
	position p;
	color::type c;
	if( !parse_fen_noclock( "rnbqk2r/1p3pp1/3bpn2/p2pN2p/P1Pp4/4P3/1P1BBPPP/RN1Q1RK1 b kq c3", p, c ) ) {
		return false;
	}

	unsigned long long old_hash = get_zobrist_hash( p );

	move m;
	if( !parse_move( p, c, "dxc3", m ) ) {
		return false;
	}

	unsigned long long new_hash_move = update_zobrist_hash( p, c, old_hash, m );

	apply_move( p, m, c );

	unsigned long long new_hash_full = get_zobrist_hash( p );

	if( new_hash_move != new_hash_full ) {
		std::cerr << "Hash mismatch: " << new_hash_full << " " << new_hash_move << std::endl;
		return false;
	}

	return true;
}


static bool do_selftest()
{
	if( !test_positions() ) {
		return false;
	}
	if( !test_zobrist() ) {
		return false;
	}
	if( !perft(6) ) {
		return false;
	}

	return true;
}
}

bool selftest()
{
	pawn_hash_table.init( PAWN_HASH_TABLE_SIZE );

	if( do_selftest() ) {
		std::cerr << "Self test passed" << std::endl;
		return true;
	}

	std::cerr << "Self test failed" << std::endl;
	abort();
	return false;
}
