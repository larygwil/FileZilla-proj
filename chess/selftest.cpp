#include "calc.hpp"
#include "chess.hpp"
#include "config.hpp"
#include "eval.hpp"
#include "eval_values.hpp"
#include "fen.hpp"
#include "moves.hpp"
#include "pawn_structure_hash_table.hpp"
#include "see.hpp"
#include "selftest.hpp"
#include "string.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <fstream>
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


template<bool split_movegen>
void perft( perft_ctx& ctx, int depth, position const& p, uint64_t& n )
{
	if( conf.depth == -1 ) {
		conf.depth = 8;
	}

	move_info* moves = ctx.move_ptr;

	check_map check( p );
	if( split_movegen ) {
		calculate_moves_captures( p, ctx.move_ptr, check );
		calculate_moves_noncaptures<false>( p, ctx.move_ptr, check );
	}
	else {
		calculate_moves( p, ctx.move_ptr, check );
	}

	if( !--depth ) {
		n += ctx.move_ptr - moves;
		ctx.move_ptr = moves;
		return;
	}

	for( move_info* it = moves; it != ctx.move_ptr; ++it ) {
		position new_pos = p;
		apply_move( new_pos, it->m );
		perft<split_movegen>( ctx, depth, new_pos, n );
	}
	ctx.move_ptr = moves;
}

template<bool split_movegen>
bool perft( std::size_t max_depth )
{
	uint64_t const perft_results[] = {
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
	for( unsigned int i = 0; i < std::min(max_depth, sizeof(perft_results)/sizeof(uint64_t)); ++i ) {
		ctx.move_ptr = ctx.moves;

		std::cerr << "Calculating number of possible moves in " << (i + 1) << " plies:" << std::endl;

		position p;

		uint64_t ret = 0;

		int max_depth = i + 1;

		timestamp start;
		perft<split_movegen>( ctx, max_depth, p, ret );
		timestamp stop;


		std::cerr << "Moves: "     << ret << std::endl;
		std::cerr << "Took:  "     << (stop - start).milliseconds() << " ms" << std::endl;
		if( ret ) {
			// Will overflow after ~3 months
			int64_t picoseconds = (stop-start).picoseconds();
			picoseconds /= ret;

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
template bool perft<true>( std::size_t max_depth );
template bool perft<false>( std::size_t max_depth );


namespace {

static bool test_move_generation( std::string const& fen, std::string const& ref_moves )
{
	position p;
	std::string error;
	if( !parse_fen_noclock( fen, p, &error ) ) {
		std::cerr << "Could not parse fen: " << error << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		return false;
	}

	move_info moves[200];
	move_info* pm = moves;
	check_map check( p );
	calculate_moves( p, pm, check );

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


static bool test_move_generation()
{
	if( !test_move_generation( "4R3/p1pp1p1p/b1n1rn2/1p2p1pP/1B1QPBr1/qPPb2PN/Pk1PNP2/R3K3 w Q g6",
						"c3-c4 f2-f3 h5-h6 h5xg6 Bb4-a5 Bb4-c5 Bb4-d6 Bb4-e7 Bb4-f8 Bb4xa3 Bf4-e3 Bf4xe5 Bf4xg5 Ke1-d1 Ke1-f1 Ne2-c1 Ne2-g1 Nh3-g1 Nh3xg5 Qd4-b6 Qd4-c4 Qd4-c5 Qd4-d5 Qd4-d6 Qd4-e3 Qd4xa7 Qd4xd3 Qd4xd7 Qd4xe5 Ra1-b1 Ra1-c1 Ra1-d1 Re8-a8 Re8-b8 Re8-c8 Re8-d8 Re8-e7 Re8-f8 Re8-g8 Re8-h8 Re8xe6" ) )
	{
		return false;
	}

	if( !test_move_generation( "r3k2r/8/2p5/1Q6/1q6/2P5/8/R3K2R w KqQk -",
						"O-O O-O-O c3xb4 Ke1-d1 Ke1-d2 Ke1-e2 Ke1-f1 Ke1-f2 Qb5-a4 Qb5-a5 Qb5-a6 Qb5-b6 Qb5-b7 Qb5-b8 Qb5-c4 Qb5-c5 Qb5-d3 Qb5-d5 Qb5-e2 Qb5-e5 Qb5-f1 Qb5-f5 Qb5-g5 Qb5-h5 Qb5xb4 Qb5xc6 Ra1-a2 Ra1-a3 Ra1-a4 Ra1-a5 Ra1-a6 Ra1-a7 Ra1-b1 Ra1-c1 Ra1-d1 Ra1xa8 Rh1-f1 Rh1-g1 Rh1-h2 Rh1-h3 Rh1-h4 Rh1-h5 Rh1-h6 Rh1-h7 Rh1xh8" ) )
	{
		return false;
	}

	if( !test_move_generation( "3k4/8/8/q2pP2K/8/8/8/8 w - d6",
						"e5-e6 Kh5-g4 Kh5-g5 Kh5-g6 Kh5-h4 Kh5-h6") )
	{
		return false;
	}

	return true;
}


static bool test_zobrist()
{
	position p;
	if( !parse_fen_noclock( "rnbqk2r/1p3pp1/3bpn2/p2pN2p/P1Pp4/4P3/1P1BBPPP/RN1Q1RK1 b kq c3", p ) ) {
		return false;
	}

	uint64_t old_hash = get_zobrist_hash( p );

	move m;
	if( !parse_move( p, "dxc3", m ) ) {
		return false;
	}

	uint64_t new_hash_move = update_zobrist_hash( p, old_hash, m );

	apply_move( p, m );

	uint64_t new_hash_full = get_zobrist_hash( p );

	if( new_hash_move != new_hash_full ) {
		std::cerr << "Hash mismatch: " << new_hash_full << " " << new_hash_move << std::endl;
		return false;
	}

	return true;
}


static bool test_lazy_eval( std::string const& fen, short& max_difference )
{
	position p;
	std::string error;
	if( !parse_fen_noclock( fen, p, &error ) ) {
		std::cerr << "Could not parse fen: " << error << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		return false;
	}

	score currents = p.white() ? p.base_eval : -p.base_eval;
	short current = currents.scale( p.material[0].mg() + p.material[1].mg() );
	short full = evaluate_full( p, p.self() );

	short diff = std::abs( full - current );

	if( diff > LAZY_EVAL ) {
		std::cerr << "Bug in lazy evaluation, full evaluation differs too much from basic evaluation:" << std::endl;
		std::cerr << "Position:   " << fen << std::endl;
		std::cerr << "Current:    " << current << std::endl;
		std::cerr << "Full:       " << full << std::endl;
		std::cerr << "Difference: " << diff << std::endl;
		return false;
	}

	max_difference = std::max( max_difference, diff );

	return true;
}


static bool test_lazy_eval()
{
	std::ifstream in_fen("test/testpositions.txt");

	short max_difference = 0;

	std::string fen;
	while( std::getline( in_fen, fen ) ) {
		if( !test_lazy_eval( fen, max_difference ) ) {
			return false;
		}
	}

	std::cout << "Max positional score difference: " << max_difference << std::endl;

	return true;
}


static bool test_pst() {
	for( int i = 0; i < 64; ++i ) {
		int opposite = (i % 8) + (7 - i / 8) * 8;
		int mirror = (i / 8) * 8 + 7 - i % 8;
		for( int p = 1; p < 7; ++p ) {
			if( pst[0][p][i] != pst[1][p][opposite] ) {
				std::cerr << "PST not symmetric for piece " << p << ", squares " << i << " and " << opposite << ": " << pst[0][p][i] << " " << pst[1][p][opposite] << std::endl;
				return false;
			}
			if( pst[0][p][i] != pst[0][p][mirror] ) {
				std::cerr << "PST not symmetric for piece " << p << ", squares " << i << " and " << mirror << ": " << pst[0][p][i] << " " << pst[0][p][mirror] << std::endl;
				return false;
			}
		}
	}

	return true;
}


static bool test_evaluation( std::string const& fen, position const& p )
{
	std::string flipped;

	std::string remaining;
	std::string first_part = split( fen, remaining );
	first_part += '/';
	while( !first_part.empty() ) {
		std::string segment = split( first_part, first_part, '/' );
		if( !flipped.empty() ) {
			flipped = '/' + flipped;
		}
		flipped = segment + flipped;
	}

	flipped += " ";
	if( remaining[0] == 'b' ) {
		flipped += "w";
	}
	else {
		flipped += "b";
	}

	if( remaining[remaining.size()] == '6' ) {
		remaining[remaining.size()] = '3';
	}
	else if( remaining[remaining.size()] == '3' ) {
		remaining[remaining.size()] = '6';
	}
	flipped += remaining.substr(1);

	for( std::size_t i = 0; i < flipped.size(); ++i ) {
		switch( flipped[i] ) {
		case 'p':
			flipped[i] = 'P';
			break;
		case 'n':
			flipped[i] = 'N';
			break;
		case 'b':
			flipped[i] = 'B';
			break;
		case 'r':
			flipped[i] = 'R';
			break;
		case 'q':
			flipped[i] = 'Q';
			break;
		case 'k':
			flipped[i] = 'K';
			break;
		case 'P':
			flipped[i] = 'p';
			break;
		case 'N':
			flipped[i] = 'n';
			break;
		case 'B':
			flipped[i] = 'b';
			break;
		case 'R':
			flipped[i] = 'r';
			break;
		case 'Q':
			flipped[i] = 'q';
			break;
		case 'K':
			flipped[i] = 'k';
			break;
		default:
			break;
		}
	}

	position p2;
	std::string error;
	if( !parse_fen_noclock( flipped, p2, &error ) ) {
		std::cerr << "Could not parse fen: " << error << std::endl;
		std::cerr << "Fen: " << flipped << std::endl;
		return false;
	}

	if( p.material[0] != p2.material[1] || p.material[1] != p2.material[0] ) {
		std::cerr << "Material not symmetric: " << p.material[0] << " " << p.material[1] << " " << p2.material[0] << " " << p2.material[1] << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		std::cerr << "Flipped: " << flipped << std::endl;
		return false;
	}

	if( p.base_eval.mg() != -p2.base_eval.mg() ||
		p.base_eval.eg() != -p2.base_eval.eg() )
	{
		std::cerr << "Base evaluation not symmetric: " << p.base_eval << " " << p2.base_eval << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		std::cerr << "Flipped: " << flipped << std::endl;
		return false;
	}

	short eval_full = evaluate_full( p, p.self() );
	short flipped_eval_full = evaluate_full( p2, p2.self() );
	if( eval_full != flipped_eval_full ) {
		std::cerr << "Evaluation not symmetric: " << eval_full << " " << flipped_eval_full << " " << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		std::cerr << explain_eval( p, p.self() ) << std::endl;
		std::cerr << "Flipped: " << flipped << std::endl;
		std::cerr << explain_eval( p2, p2.self() ) << std::endl;
		return false;
	}


	return true;
}


static void test_moves_noncaptures( std::string const& fen, position const& p )
{
	check_map check( p );
	if( check.check ) {
		return;
	}

	move_info moves_full[200];
	move_info* move_ptr_full = moves_full;
	move_info moves_check[200];
	move_info* move_ptr_check = moves_check;

	calculate_moves_noncaptures<false>( p, move_ptr_full, check );
	calculate_moves_noncaptures<true>( p, move_ptr_check, check );

	for( move_info* it = moves_full; it != move_ptr_full; ++it ) {
		position p2 = p;
		apply_move( p2, it->m );
		check_map new_check( p2 );
		if( new_check.check ) {
			move_info* it2;
			for( it2 = moves_check; it2 != move_ptr_check; ++it2 ) {
				if( it->m == it2->m ) {
					break;
				}
			}
			if( it2 == move_ptr_check ) {
				std::cerr << "Checking move not generated by calculate_moves_noncaptures<true>" << std::endl;
				std::cerr << "Fen: " << fen << std::endl;
				std::cerr << "Move: " << move_to_string( it->m ) << std::endl;
				abort();
			}
		}
	}
}


static void process_test_positions()
{
	std::ifstream in_fen("test/testpositions.txt");

	std::string fen;
	while( std::getline( in_fen, fen ) ) {
		position p;
		std::string error;
		if( !parse_fen_noclock( fen, p, &error ) ) {
			std::cerr << "Could not parse fen: " << error << std::endl;
			std::cerr << "Fen: " << fen << std::endl;
			abort();
		}

		if( !test_evaluation( fen, p ) ) {
			abort();
		}

		test_moves_noncaptures( fen, p );
	}
}


static bool do_selftest()
{
	if( !test_pst() ) {
		return false;
	}
	process_test_positions();
	if( !test_move_generation() ) {
		return false;
	}
	if( !test_zobrist() ) {
		return false;
	}
	if( !test_lazy_eval() ) {
		return false;
	}
	if( !perft<true>(6) ) {
		return false;
	}
	if( !perft<false>(6) ) {
		return false;
	}

	return true;
}

static void check_popcount( uint64_t v, uint64_t expected )
{
	uint64_t c = popcount(v);
	if( c != expected ) {
		std::cerr << "Popcount failed on " << v << ", got " << c << ", expected " << expected << std::endl;
		abort();
	}
}

static void check_popcount()
{
	for( unsigned int i = 0; i < 64; ++i ) {
		uint64_t v = 1ull << i;
		check_popcount( v, 1 );

		check_popcount( v - 1, i );
	}
}

static void check_bitscan( uint64_t v, uint64_t expected_count, uint64_t expected_sum )
{
	uint64_t c = 0;
	uint64_t sum = 0;
	uint64_t v2 = v;
	while( v2 ) {
		++c;
		uint64_t i = bitscan_unset( v2 );
		sum += i;
	}

	if( c != expected_count || sum != expected_sum ) {
		std::cerr << "Bitscan failed on " << v << ", got " << c << ", " << sum << ", expected " << expected_count << ", " << expected_sum << std::endl;
		abort();
	}

}

static void check_bitscan()
{
	check_bitscan( 0x5555555555555555ull, 32, 992 );
}

static void check_see()
{
	std::string const fen = "r3r3/p4ppp/4k3/R3n3/1N1KP3/8/6BP/8 w - -";
	std::string const ms = "Rxe5";

	position p;
	std::string error;
	if( !parse_fen_noclock( fen, p, &error ) ) {
		std::cerr << "Could not parse fen: " << error << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		abort();
	}

	move m;
	if( !parse_move( p, ms, m ) ) {
		abort();
	}

	short v = see( p, m );
	if( v <= 0 ) {
		std::cerr << "See of " << fen << " " << ms << " needs to be bigger than 0, but is " << v << std::endl;
		abort();
	}
}


void do_check_disambiguation( std::string const& fen, std::string const& ms, std::string const& ref )
{
	position p;
	std::string error;
	if( !parse_fen_noclock( fen, p, &error ) ) {
		std::cerr << "Could not parse fen: " << error << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		abort();
	}

	move m;
	if( !parse_move( p, ms, m ) ) {
		abort();
	}

	std::string const san = move_to_san( p, m );
	if( san != ref ) {
		std::cerr << "Could not obtain SAN of move." << std::endl;
		std::cerr << "Actual: " << san << std::endl;
		std::cerr << "Expected: " << ref << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		abort();
	}
}


void check_disambiguation()
{
	do_check_disambiguation( "2K5/8/8/4N3/8/8/8/2k3N1 w - - 0 1", "Ngf3", "Ngf3" );
	do_check_disambiguation( "2K5/8/8/4N3/8/8/8/2k3N1 w - - 0 1", "N1f3", "Ngf3" );
	do_check_disambiguation( "2K5/8/8/4N3/8/8/8/2k1N3 w - - 0 1", "Ne1f3", "N1f3" );
	do_check_disambiguation( "2K5/8/8/4N3/8/8/8/2k1N3 w - - 0 1", "N1f3", "N1f3" );
	do_check_disambiguation( "2K5/8/8/4N3/8/8/8/2k1N1N1 w - - 0 1", "Ne1f3", "Ne1f3" );
	do_check_disambiguation( "2K5/8/8/4N3/8/8/8/2k1N1N1 w - - 0 1", "Ne5f3", "N5f3" );
}
}

bool selftest()
{
	check_popcount();
	check_bitscan();
	check_see();

	pawn_hash_table.init( conf.pawn_hash_table_size );

	check_disambiguation();

	if( do_selftest() ) {
		std::cerr << "Self test passed" << std::endl;
		return true;
	}

	std::cerr << "Self test failed" << std::endl;
	abort();
	return false;
}
