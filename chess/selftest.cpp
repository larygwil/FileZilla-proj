#include "calc.hpp"
#include "chess.hpp"
#include "fen.hpp"
#include "moves.hpp"
#include "selftest.hpp"
#include "util.hpp"
#include "pawn_structure_hash_table.hpp"
#include "config.hpp"
#include "zobrist.hpp"
#include "eval.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <vector>

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
	std::string error;
	if( !parse_fen_noclock( fen, p, c, &error ) ) {
		std::cerr << "Could not parse fen: " << error << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
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

	if( !test_position( "3k4/8/8/q2pP2K/8/8/8/8 w - d6",
						"e5-e6 Kh5-g4 Kh5-g5 Kh5-g6 Kh5-h4 Kh5-h6") )
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


static bool test_lazy_eval( std::string const& fen )
{
	position p;
	color::type c;
	std::string error;
	if( !parse_fen_noclock( fen, p, c, &error ) ) {
		std::cerr << "Could not parse fen: " << error << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		return false;
	}

	short current = evaluate_fast( p, c );
	short full = evaluate_full( p, c );

	short diff = std::abs( full - current );

	if( diff > LAZY_EVAL ) {
		std::cerr << "Bug in lazy evaluation, full evaluation differs too much from basic evaluation:" << std::endl;
		std::cerr << "Position:   " << fen << std::endl;
		std::cerr << "Current:    " << current << std::endl;
		std::cerr << "Full:       " << full << std::endl;
		std::cerr << "Difference: " << diff << std::endl;
		return false;
	}

	return true;
}


static bool test_lazy_eval()
{
	std::string const data[] = {
		"1r2kr2/p3q2p/8/3b4/3P4/8/P1P1N2P/3QKB1q b - -",
		"1n3b1r/rpB1kppp/p3bN2/8/8/4Q3/PPP1B1PP/2KR2NR w - -",
		"r1b3nr/pp1kbQpp/8/8/3NP2P/PN2P1B1/1P4P1/R4K1R w - -",
		"Q1bk3r/p2n1pp1/7p/2N1p3/3Q3P/P7/1P3PP1/2R1KBNR w K -",
		"r1b2b1r/p3kppp/8/4NB2/8/1N2P1P1/PP3PP1/n2Q1K1R w - -",
		"r1b2b1r/1p2k2p/4q1B1/p3Q3/8/4P1P1/PPP3PP/2KR1R2 w - -",
		"r1b2b1r/pp2kppp/8/4NB2/8/1N2P1P1/PP3PP1/n2Q1K1R w - -",
		"r1bk3r/pp3ppp/3b4/4NB2/8/1N2P1P1/PP3PP1/n2Q1K1R w - -",
		"rnb2bnr/ppppkppp/8/4P3/4P3/2N5/PPP2PPR/R1BQKBN1 w Q -",
		"1rb2b1r/pp1pkppp/8/3Q3n/3N4/1N1PP1B1/PP4PP/R4RK1 w - -",
		"r1b1kb1r/pp1n1ppp/3PB3/8/Q3PB2/5N2/PP1NKPPP/n6R w kq -",
		"r1b1kb1r/pp1pnppp/8/1N6/4Q3/1N2P1B1/PP3P1P/2KR3R w kq -",
		"r1b1kbr1/ppBpnppp/8/8/4Q3/1N2PN2/PPP2PPP/R3KB1R w KQq -",
		"r1b2b1r/pp1pkpp1/7p/3Q3n/3N4/1N1PP1B1/PP4PP/R4RK1 w - -",
		"rnb2bnr/ppppkppp/8/4P3/4P3/1PN5/1PP2PPP/R1BQKBNR w KQ -",
		"rnbqkb1Q/p1pppp1p/8/3P2p1/p4B2/8/1PP2PPP/RN2KBNn w Qq -",
		"r1b1kb1r/pp1n1ppp/3PB3/8/Q3PB2/5N2/PP1N1PPP/n4K1R w kq -",
		"r1b1kb1r/ppBpnppp/4Q3/8/8/1N2PN2/PPP2PPP/R3KB1R w KQkq -",
		"rnb2bnr/ppppkppp/8/4P3/4P3/2N3P1/PPP2PP1/R1BQKBNR w KQ -",
		"rnbq1br1/ppp1pppp/2kB4/7Q/3PP3/2N5/PPP2PPP/R3KBNR w KQ -",
		"r1b2b1r/pp1kPppp/2n5/3n4/4NB2/4PN2/PPP2PPP/R2QKB1R w KQ -",
		"rnbk1b1r/pp1ppppp/1np5/Q5B1/3PP3/1P6/P1P2PPP/RN2KBNR w KQ -",
		"r1bk1br1/pp1nppp1/5N1p/1B6/1P1Q1B1P/4P3/2P2PP1/R3K1NR w KQ -",
		"r1bqkbnr/p1pnpp1p/3p4/1B4P1/3PPB2/8/PPP2PP1/RN1QK1NR w KQkq -",
		""
	};

	for( std::string const* p = data; !p->empty(); ++p ) {
		if( !test_lazy_eval( *p ) ) {
			return false;
		}
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
	if( !test_lazy_eval() ) {
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
	pawn_hash_table.init( conf.pawn_hash_table_size );

	if( do_selftest() ) {
		std::cerr << "Self test passed" << std::endl;
		return true;
	}

	std::cerr << "Self test failed" << std::endl;
	abort();
	return false;
}
