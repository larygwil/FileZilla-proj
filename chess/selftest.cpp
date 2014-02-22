#include "calc.hpp"
#include "chess.hpp"
#include "config.hpp"
#include "endgame.hpp"
#include "eval.hpp"
#include "eval_values.hpp"
#include "fen.hpp"
#include "moves.hpp"
#include "pawn_structure_hash_table.hpp"
#include "random.hpp"
#include "see.hpp"
#include "selftest.hpp"
#include "util/logger.hpp"
#include "util/mutex.hpp"
#include "util/time.hpp"
#include "util/string.hpp"
#include "util/thread.hpp"
#include "util.hpp"

#include <array>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <memory>
#include <vector>


namespace {
void checking( std::string const& what, bool endline = false )
{
	std::cout << "Checking " << what << "... ";
	if( endline ) {
		std::cout << std::endl;
	}
	else {
		std::cout.flush();
	}
}

void pass()
{
	std::cout << "pass" << std::endl;
}
}

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
	move_info* moves = ctx.move_ptr;

	check_map check( p );
	if( split_movegen ) {
		calculate_moves<movegen_type::capture>( p, ctx.move_ptr, check );
		calculate_moves<movegen_type::noncapture>( p, ctx.move_ptr, check );
	}
	else {
		calculate_moves<movegen_type::all>( p, ctx.move_ptr, check );
	}

	if( !--depth ) {
		n += ctx.move_ptr - moves;
		ctx.move_ptr = moves;
		return;
	}

	for( move_info* it = moves; it != ctx.move_ptr; ++it ) {
		position new_pos(p);
		apply_move( new_pos, it->m );
		perft<split_movegen>( ctx, depth, new_pos, n );
	}
	ctx.move_ptr = moves;
}

template<bool split_movegen>
void perft( position const& p, std::vector<uint64_t> const& expected, std::size_t max_depth = 0, bool verbose = true )
{
	if( !max_depth ) {
		max_depth = expected.size();;
	}

	perft_ctx ctx;
	for( unsigned int i = 0; i < max_depth; ++i ) {

		ctx.move_ptr = ctx.moves;

		if( verbose ) {
			std::cerr << "Calculating number of possible moves in " << (i + 1) << " plies:" << std::endl;
		}

		uint64_t ret = 0;

		int max_depth = i + 1;

		timestamp start;
		perft<split_movegen>( ctx, max_depth, p, ret );
		timestamp stop;

		duration elapsed = stop - start;

		if( verbose ) {
			std::cerr << "Moves:     "     << ret;
			if( i < expected.size() && expected[i] == ret ) {
				std::cerr << " (pass)";
			}

			std::cerr << std::endl;
			std::cerr << "Took:      "     << elapsed.milliseconds() << " ms" << std::endl;
			if( ret ) {
				// Will overflow after ~3 months
				int64_t picoseconds = elapsed.picoseconds();
				picoseconds /= ret;

				std::stringstream ss;
				ss << "Time/move: " << picoseconds / 1000 << "." << std::setw(1) << std::setfill('0') << (picoseconds / 100) % 10 << " ns" << std::endl;

				if( !elapsed.empty() ) {
					ss << "Moves/s:   " << elapsed.get_items_per_second(ret) << std::endl;
				}

				std::cerr << ss.str();
			}
		}

		if( i < expected.size() && expected[i] != 0 && ret != expected[i] ) {
			std::cerr << "FAIL! Expected " << expected[i] << " moves." << std::endl;
			abort();
		}

		if( verbose ) {
			std::cerr << std::endl;
		}
	}
}

void perft( position const& p )
{
	perft<false>( p, {}, 999999, true );
}

namespace {

position test_parse_fen( context const& ctx, std::string const& fen )
{
	position p;
	std::string error;
	if( !parse_fen( ctx.conf_, fen, p, &error ) ) {
		std::cerr << "Could not parse fen: " << error << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		abort();
	}

	return p;
}

move test_parse_move( position const& p, std::string const& ms )
{
	move m;
	std::string error;
	if( !parse_move( p, ms, m, error ) ) {
		std::cerr << error << ": " << ms << std::endl;
		abort();
	}

	return m;
}

static void test_move_generation( context& ctx, std::string const& fen, std::string const& ref_moves )
{
	position p = test_parse_fen( ctx, fen );

	std::vector<std::string> ms;
	for( auto m : calculate_moves<movegen_type::all>( p ) ) {
		ms.push_back( move_to_string( p, m ) );
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
		abort();
	}
}


static void test_move_generation( context& ctx )
{
	checking("move generation");

	test_move_generation( ctx, "4R3/p1pp1p1p/b1n1rn2/1p2p1pP/1B1QPBr1/qPPb2PN/Pk1PNP2/R3K3 w Q g6",
						"c3-c4 f2-f3 h5-h6 h5xg6 Bb4-a5 Bb4-c5 Bb4-d6 Bb4-e7 Bb4-f8 Bb4xa3 Bf4-e3 Bf4xe5 Bf4xg5 Ke1-d1 Ke1-f1 Ne2-c1 Ne2-g1 Nh3-g1 Nh3xg5 Qd4-b6 Qd4-c4 Qd4-c5 Qd4-d5 Qd4-d6 Qd4-e3 Qd4xa7 Qd4xd3 Qd4xd7 Qd4xe5 Ra1-b1 Ra1-c1 Ra1-d1 Re8-a8 Re8-b8 Re8-c8 Re8-d8 Re8-e7 Re8-f8 Re8-g8 Re8-h8 Re8xe6" );
	test_move_generation( ctx, "r3k2r/8/2p5/1Q6/1q6/2P5/8/R3K2R w KqQk -",
						"O-O O-O-O c3xb4 Ke1-d1 Ke1-d2 Ke1-e2 Ke1-f1 Ke1-f2 Qb5-a4 Qb5-a5 Qb5-a6 Qb5-b6 Qb5-b7 Qb5-b8 Qb5-c4 Qb5-c5 Qb5-d3 Qb5-d5 Qb5-e2 Qb5-e5 Qb5-f1 Qb5-f5 Qb5-g5 Qb5-h5 Qb5xb4 Qb5xc6 Ra1-a2 Ra1-a3 Ra1-a4 Ra1-a5 Ra1-a6 Ra1-a7 Ra1-b1 Ra1-c1 Ra1-d1 Ra1xa8 Rh1-f1 Rh1-g1 Rh1-h2 Rh1-h3 Rh1-h4 Rh1-h5 Rh1-h6 Rh1-h7 Rh1xh8" );
	test_move_generation( ctx, "3k4/8/8/q2pP2K/8/8/8/8 w - d6",
						"e5-e6 Kh5-g4 Kh5-g5 Kh5-g6 Kh5-h4 Kh5-h6");
	test_move_generation( ctx, "3k4/8/8/q2pPP1K/8/8/8/8 w - d6",
						"e5-e6 e5xd6 f5-f6 Kh5-g4 Kh5-g5 Kh5-g6 Kh5-h4 Kh5-h6");
	test_move_generation( ctx, "3k4/8/8/K2pP2q/8/8/8/8 w - d6",
						"e5-e6 Ka5-a4 Ka5-a6 Ka5-b4 Ka5-b5 Ka5-b6");
	test_move_generation( ctx, "3k4/8/8/K2pPP1q/8/8/8/8 w - d6",
						"e5-e6 e5xd6 f5-f6 Ka5-a4 Ka5-a6 Ka5-b4 Ka5-b5 Ka5-b6");
	test_move_generation( ctx, "r1bqkb1r/ppp1ppPp/2n5/8/8/5N2/PpPP1PPP/R1BQKB1R w KQkq -",
						"a2-a3 a2-a4 c2-c3 c2-c4 d2-d3 d2-d4 g2-g3 g2-g4 g7-g8=B g7-g8=N g7-g8=Q g7-g8=R g7xf8=B g7xf8=N g7xf8=Q g7xf8=R g7xh8=B g7xh8=N g7xh8=Q g7xh8=R h2-h3 h2-h4 Bc1xb2 Bf1-a6 Bf1-b5 Bf1-c4 Bf1-d3 Bf1-e2 Ke1-e2 Nf3-d4 Nf3-e5 Nf3-g1 Nf3-g5 Nf3-h4 Qd1-e2 Ra1-b1 Rh1-g1");
	test_move_generation( ctx, "8/5Rp1/6r1/4p2p/RPp1k2b/4B3/7P/5K2 b - b3 0 1",
						"c4-c3 Bh4-d8 Bh4-e1 Bh4-e7 Bh4-f2 Bh4-f6 Bh4-g3 Bh4-g5 Ke4-d3 Ke4-d5 Ke4xe3 Rg6-a6 Rg6-b6 Rg6-c6 Rg6-d6 Rg6-e6 Rg6-f6 Rg6-g1 Rg6-g2 Rg6-g3 Rg6-g4 Rg6-g5 Rg6-h6");
	test_move_generation( ctx, "8/5bk1/8/2Pp4/8/1K6/8/8 w - d6 0 1",
						"c5-c6 Kb3-a2 Kb3-a3 Kb3-a4 Kb3-b2 Kb3-b4 Kb3-c2 Kb3-c3" );
	test_move_generation( ctx, "r1b1k2r/ppq2ppp/4pn2/1p1pP3/3P4/2N2P2/PPP3PK/R1BQ1R2 w kq d6 0 1",
						"a2-a3 a2-a4 b2-b3 b2-b4 e5xd6 f3-f4 g2-g3 g2-g4 Bc1-d2 Bc1-e3 Bc1-f4 Bc1-g5 Bc1-h6 Kh2-g1 Kh2-g3 Kh2-h1 Kh2-h3 Nc3-a4 Nc3-b1 Nc3-e2 Nc3-e4 Nc3xb5 Nc3xd5 Qd1-d2 Qd1-d3 Qd1-e1 Qd1-e2 Ra1-b1 Rf1-e1 Rf1-f2 Rf1-g1 Rf1-h1" );

	pass();
}


static void test_move_legality_check( context& ctx, std::string const& fen, move const& m, bool ref_valid )
{
	position p = test_parse_fen( ctx, fen );

	check_map check( p );
	bool valid = is_valid_move( p, m, check );
	if( valid != ref_valid ) {
		std::cerr << "Validity mismatch!" << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		std::cerr << "Move: " << move_to_string(p, m) << std::endl;
		std::cerr << "Reference: " << ref_valid << std::endl;
		std::cerr << "Actual: " << valid << std::endl;
		abort();
	}
}

static void test_move_legality_check( context& ctx )
{
	checking("move legality test");

	test_move_legality_check( ctx, "rnbqk2r/pppp1ppp/5n2/2b1p3/4P3/2Q5/PPPP1PPP/RNB1KBNR w KQkq - 0 1", move(square::c3, square::c6, 0), false );
	test_move_legality_check( ctx, "rnbqk2r/pppp1ppp/5n2/2b1p3/4P3/2Q5/PPPP1PPP/RNB1KBNR w KQkq - 0 1", move(square::c2, square::c4, 0), false );
	test_move_legality_check( ctx, "rnbqk2r/pppp1ppp/5n2/2b1p3/4P3/2Q5/PPPP1PPP/RNB1KBNR w KQkq - 0 1", move(square::c2, square::c3, 0), false );
	test_move_legality_check( ctx, "rnbqk2r/pppp1ppp/5n2/2b1p3/4P3/2Q5/PPPP1PPP/RNB1KBNR w KQkq - 0 1", move(square::b2, square::c3, 0), false );
	test_move_legality_check( ctx, "rnbqk2r/pppp1ppp/5n2/2b1p3/4P3/2Q5/PPPP1PPP/RNB1KBNR w KQkq - 0 1", move(square::a2, square::b2, 0), false );
	test_move_legality_check( ctx, "rnbqk2r/pppp1ppp/5n2/2b1p3/4P3/2Q5/PPPP1PPP/RNB1KBNR w KQkq - 0 1", move(square::a2, square::b5, 0), false );
	test_move_legality_check( ctx, "rnbqk2r/pppp1ppp/5n2/2b1p3/4P3/2Q5/PPPP1PPP/RNB1KBNR w KQkq - 0 1", move(square::a2, square::c3, 0), false );
	test_move_legality_check( ctx, "8/5bk1/8/2Pp4/8/1K6/8/8 w - d6 0 1", move( square::c5, square::d6, move_flags::enpassant), false );
	test_move_legality_check( ctx, "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQK2R b KQkq - 0 1", move(square::e8, square::g8, move_flags::castle), true);
	test_move_legality_check( ctx, "r1bqk2r/pppp1ppp/2n1pn2/2B1P3/5P2/2N2N2/PPPP2PP/R2QKB1R b KQkq - 0 1", move(square::e8, square::g8, move_flags::castle), false);

	ctx.conf_.fischer_random = true;
	test_move_legality_check( ctx, "bqnb1rkr/p2p1ppp/1p3n2/2p1p3/8/1NPN4/PPBPPPPP/BQ3RKR w HFhf c6 0 5", move(square::g1, square::c1, move_flags::castle), true);
	test_move_legality_check( ctx, "8/8/8/8/8/8/4k3/R5K1 w A - 0 1", move(square::g1, square::c1, move_flags::castle), false);
	test_move_legality_check( ctx, "8/8/8/8/8/8/1k6/R5K1 w A - 0 1", move(square::g1, square::c1, move_flags::castle), false);
	test_move_legality_check( ctx, "8/8/8/8/8/1k6/8/R5K1 w A - 0 1", move(square::g1, square::c1, move_flags::castle), true);
	test_move_legality_check( ctx, "bb1rk1rq/pppppppp/3nn3/8/4P3/3N2N1/PPPP1PPP/BB1RK1RQ b DGdg -", move(square::e8, square::g8, move_flags::castle), true);
	test_move_legality_check( ctx, "bb1rr1kq/pppppppp/4n3/4PN2/8/nP1N4/P1PP1PPP/BB1RK1RQ w DG -", move(square::e1, square::g1, move_flags::castle), true);
	test_move_legality_check( ctx, "1k2r1bq/rppp1ppp/3nn3/4p3/4P2P/3N1P2/PPPP3P/RK1BRN1Q b AEe -", move(square::b8, square::c8, move_flags::castle), false);
	test_move_legality_check( ctx, "rk1br1bq/ppppp2p/3nn1p1/5p2/8/3NPBN1/PPPP1PPP/RK2R1BQ w AEae -", move( square::b1, square::c1, move_flags::castle), true );
	ctx.conf_.fischer_random = false;

	pass();
}

static void test_zobrist( context& ctx, std::string const& fen, std::string const& ms )
{
	position p = test_parse_fen( ctx, fen );

	move m = test_parse_move( p, ms );

	apply_move( p, m );

	uint64_t new_hash_move = p.hash_;
	p.update_derived();
	uint64_t new_hash_full = p.hash_;

	if( new_hash_move != new_hash_full ) {
		std::cerr << "Hash mismatch: " << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		std::cerr << "Move: " << ms << std::endl;
		std::cerr << "Full: " << new_hash_full << std::endl;
		std::cerr << "Incremental: " << new_hash_move << std::endl;
		abort();
	}
}

static void test_zobrist( context& ctx )
{
	checking("zobrist hashing");

	test_zobrist( ctx, "rnbqk2r/1p3pp1/3bpn2/p2pN2p/P1Pp4/4P3/1P1BBPPP/RN1Q1RK1 b kq c3", "dxc3" );
	test_zobrist( ctx, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", "e4" );
	test_zobrist( ctx, "rn1qkbnr/pppb2pp/5p2/1B1pp3/4P3/2N2N2/PPPP1PPP/R1BQK2R w KQkq e6 0 5", "O-O" );
	test_zobrist( ctx, "r3kbnr/pp1n2pp/1qp2p2/3pp2b/4P3/PPN2N1P/2PPBPP1/R1BQR1K1 b kq - 2 10", "O-O-O" );

	pass();
}

static bool test_lazy_eval( context& ctx, std::string const& fen, short& max_difference )
{
	position p = test_parse_fen( ctx, fen );

	score currents = p.white() ? p.base_eval : -p.base_eval;
	short current = currents.scale( p.material[0].mg() + p.material[1].mg() );
	short full = evaluate_full( ctx.pawn_tt_, p );

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


static void check_pawn_shield()
{
	// In all positions, white has a pawn shield _at least_ as good as black.
	std::string fens[] = {
		"6k1/5p1p/8/8/8/8/5PPP/6K1 w - - 0 1",
		"6k1/5p1p/6p1/8/8/8/5PPP/6K1 w - - 0 1",
		"6k1/7p/5pp1/8/8/6P1/5P1P/6K1 w - - 0 1",
		"6k1/7p/5pp1/8/8/8/5PPP/6K1 w - - 0 1",
		"6k1/6pp/5p2/8/8/8/5PPP/6K1 w - - 0 1",
		"6k1/8/5ppp/8/8/6PP/5P2/6K1 w - - 0 1",
		"6k1/8/5ppp/8/8/5P1P/6P1/6K1 w - - 0 1",
		"6k1/5p2/6pp/8/8/6P1/5P1P/6K1 w - - 0 1",
		"6k1/5p2/6pp/8/8/7P/5PP1/6K1 w - - 0 1",
		"8/5pkp/6p1/8/8/6P1/5P1P/6K1 w - - 0 1",
		"1k6/p1p5/1p6/1P4p1/8/6P1/5P1P/6K1 w - - 0 1"
	};

	for( auto const& fen : fens ) {
		position p;
		if( !parse_fen( config(), fen, p ) ) {
			abort();
		}

		score w = evaluate_pawn_shield( p, color::white );
		score b = evaluate_pawn_shield( p, color::black );
		if( w.mg() < b.mg() || w.mg() < b.mg() ) {
			std::cerr << "White pawn shield is smaller than black's: " << w << " " << b << "\n";
			std::cerr << fen << "\n";
			abort();
		}

	}
}


static void check_eval()
{
	checking("evaluation");
	bool changed = false;
	if( !eval_values::sane_base( changed ) || changed ) {
		std::cerr << "Evaluation values not sane" << std::endl;
		abort();
	}
	if( !eval_values::sane_derived() ) {
		std::cerr << "Evaluation values not sane" << std::endl;
		abort();
	}
	if( eval_values::normalize() ) {
		std::cerr << "Evaluation values not normalized" << std::endl;
		abort();
	}

	check_pawn_shield();

	pass();
}


static void test_lazy_eval( context& ctx )
{
	checking("lazy evaluation");
	std::ifstream in_fen(ctx.conf_.self_dir + "test/testpositions.txt");

	short max_difference = 0;

	std::string fen;
	while( std::getline( in_fen, fen ) ) {
		if( !test_lazy_eval( ctx, fen, max_difference ) ) {
			abort();
		}
	}

	pass();
	std::cout << "Max positional score difference: " << max_difference << std::endl;
}


static void test_pst() {
	checking("pst symmetry");
	for( int i = 0; i < 64; ++i ) {
		int opposite = (i % 8) + (7 - i / 8) * 8;
		int mirror = (i / 8) * 8 + 7 - i % 8;
		for( int p = 1; p < 7; ++p ) {
			if( pst(color::white, static_cast<pieces::type>(p), i) != pst(color::black, static_cast<pieces::type>(p), opposite) ) {
				std::cerr << "PST not symmetric for piece " << p << ", squares " << i << " and " << opposite << ": " << pst(color::white, static_cast<pieces::type>(p), i) << " " << pst(color::black, static_cast<pieces::type>(p), opposite) << std::endl;
				abort();
			}
			if( pst(color::white, static_cast<pieces::type>(p), i) != pst(color::white, static_cast<pieces::type>(p), mirror) ) {
				std::cerr << "PST not symmetric for piece " << p << ", squares " << i << " and " << mirror << ": " << pst(color::white, static_cast<pieces::type>(p), i) << " " << pst(color::white, static_cast<pieces::type>(p), mirror) << std::endl;
				abort();
			}
		}
	}
	pass();
}


std::string flip_fen( std::string const& fen )
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

	if( remaining[remaining.size() - 1] == '6' ) {
		remaining[remaining.size() - 1] = '3';
	}
	else if( remaining[remaining.size() - 1] == '3' ) {
		remaining[remaining.size() - 1] = '6';
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

	return flipped;
}


static bool test_evaluation( context& ctx, std::string const& fen, position const& p )
{
	std::string flipped = flip_fen( fen );

	position p2 = test_parse_fen( ctx, flipped );

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

	short eval_full = evaluate_full( ctx.pawn_tt_, p );
	short flipped_eval_full = evaluate_full( ctx.pawn_tt_, p2 );
	if( eval_full != flipped_eval_full ) {
		std::cerr << "Evaluation not symmetric: " << eval_full << " " << flipped_eval_full << " " << std::endl;
		std::cerr << "Fen: " << fen << std::endl;
		std::cerr << explain_eval( ctx.pawn_tt_, p ) << std::endl;
		std::cerr << "Flipped: " << flipped << std::endl;
		std::cerr << explain_eval( ctx.pawn_tt_, p2 ) << std::endl;
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

	auto noncaptures = calculate_moves<movegen_type::noncapture>( p, check );
	auto pseudocheck = calculate_moves<movegen_type::pseudocheck>( p, check );
	
	for( auto m : noncaptures ) {
		position p2 = p;
		apply_move( p2, m );
		check_map new_check( p2 );

		auto it = pseudocheck.begin();
		for( ; it != pseudocheck.end(); ++it ) {
			if( *it == m ) {
				break;
			}
		}

		if( new_check.check && it == pseudocheck.end() ) {
			std::cerr << "Checking move not generated by calculate_moves_noncaptures<true>" << std::endl;
			std::cerr << "Fen: " << fen << std::endl;
			std::cerr << "Move: " << move_to_string( p, m ) << std::endl;
			abort();
		}
		
		if( it != pseudocheck.end() ) {
			pseudocheck.erase( it );
		}
	}

	if( !pseudocheck.empty() ) {
		abort();
	}	
}


static void test_incorrect_positions( context& ctx )
{
	checking("fen validation");

	auto fens = {
		"8/8/8/8/8/8/8/8 w - -",
		"k7/8/8/8/8/8/8/8 w - -",
		"K7/8/8/8/8/8/8/8 w - -",
		"Kk6/8/8/8/8/8/8/8 w - -",
		"kQK5/8/8/8/8/8/8/8 w - -",
		"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e4",
		"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e2",
		"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq f3",
		"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq e3",
		"rnbqkbnr/pppppppp/8/8/4P3/8/PPPPPPPP/RNBQKBNR b KQkq e3",
		"rnbqkbnr/pppppppp/8/8/4P3/4P3/PPPP1PPP/RNBQKBNR b KQkq e3",
		"rnbqkbnr/pppppppp/8/8/8/8/PPPP1PPP/RNBQKBNR b KQkq e3",
		"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e6",
		"r2k3r/8/8/8/8/8/8/R3K2R w KQkq -",
		"r3k2r/8/8/8/8/8/8/R2K3R w KQkq -",
		"1r2k2r/8/8/8/8/8/8/R3K2R w KQkq -",
		"r3k2r/8/8/8/8/8/8/1R2K2R w KQkq -",
		"r3k1r1/8/8/8/8/8/8/R3K2R w KQkq -",
		"r3k2r/8/8/8/8/8/8/R3K1R1 w KQkq -"
	};

	for( auto const& fen : fens ) {
		position p;
		std::string error;
		if( parse_fen( ctx.conf_, fen, p, &error ) ) {
			std::cerr << "Fen of invalid position was not rejected." << std::endl;
			std::cerr << "Fen: " << fen << std::endl;
			abort();
		}
	}

	pass();
}


static void process_test_positions( context& ctx )
{
	std::ifstream in_fen(ctx.conf_.self_dir + "test/testpositions.txt");

	std::string fen;
	while( std::getline( in_fen, fen ) ) {
		position p = test_parse_fen( ctx, fen );

		if( !test_evaluation( ctx, fen, p ) ) {
			abort();
		}

		test_moves_noncaptures( fen, p );
	}
}

static void test_perft( context& ctx, std::string const& fen, std::vector<uint64_t> expected )
{
	position p = test_parse_fen( ctx, fen );
	
	perft<true>(p, expected, 0, false);
	std::cerr << ".";
	perft<false>(p, expected, 0, false);
	std::cerr << ".";
}

typedef std::pair<std::string,std::vector<uint64_t>> perft_data_entry_t;
typedef std::vector<perft_data_entry_t> perft_data_t;
class perft_thread : public thread
{
public:
	perft_thread( perft_data_t const& data, perft_data_t::const_iterator& it, mutex& m )
		: data_(data)
		, it_(it)
		, m_(m)
	{
	}

	virtual ~perft_thread()
	{
		join();
	}

	virtual void onRun()
	{
		context ctx;
		ctx.pawn_tt_.init(1);

		scoped_lock l(m_);
		while( it_ != data_.end() ) {
			perft_data_t::value_type d = *it_++;

			l.unlock();

			test_perft( ctx, d.first, d.second );

			l.lock();
		}
	}

	perft_data_t const& data_;
	perft_data_t::const_iterator& it_;
	mutex& m_;
};

static void test_perft( context& ctx )
{
	checking( "perft" );

	perft_data_t perft_data;
	// Looking forward to C++14 here...
	perft_data.push_back( {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq -", {20,400,8902,197281,4865609,119060324}} );//,3195901860ull,84998978956ull
	perft_data.push_back( {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq -", {6,264,9467,422333,15833292,706045033}} );
	perft_data.push_back( {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", {14,191,2812,43238,674624,11030083,178633661}} );
	perft_data.push_back( {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", {48,2039,97862,4085603,193690690}} );
	perft_data.push_back( {"rnbqkb1r/pp1p1ppp/2p5/4P3/2B5/8/PPP1NnPP/RNBQK2R w KQkq -", {42,1352,53392}} );
	perft_data.push_back( {"8/5bk1/8/2Pp4/8/1K6/8/8 w - d6", {8,104,736,9287,62297,824064}} );
	perft_data.push_back( {"8/8/1k6/2b5/2pP4/8/5K2/8 b - d3", {15,126,1928,13931,206379,1440467}} );
	perft_data.push_back( {"5k2/8/8/8/8/8/8/4K2R w K -", {15,66,1198,6399,120330,661072}} );
	perft_data.push_back( {"3k4/8/8/8/8/8/8/R3K3 w Q -", {16,71,1286,7418,141077,803711}} );
	perft_data.push_back( {"r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq -", {26,1141,27826,1274206}} );
	perft_data.push_back( {"r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq -", {44,1494,50509,1720476}} );
	perft_data.push_back( {"2K2r2/4P3/8/8/8/8/8/3k4 w - -", {11,133,1442,19174,266199,3821001}} );
	perft_data.push_back( {"8/8/1P2K3/8/2n5/1q6/8/5k2 b - -", {29,165,5160,31961,1004658}} );
	perft_data.push_back( {"4k3/1P6/8/8/8/8/K7/8 w - -", {9,40,472,2661,38983,217342}} );
	perft_data.push_back( {"8/P1k5/K7/8/8/8/8/8 w - -", {6,27,273,1329,18135,92683}} );
	perft_data.push_back( {"K1k5/8/P7/8/8/8/8/8 w - -", {2,6,13,63,382,2217}} );
	perft_data.push_back( {"8/k1P5/8/1K6/8/8/8/8 w - -", {10,25,268,926,10857,43261,567584 }} );
	perft_data.push_back( {"8/8/2k5/5q2/5n2/8/5K2/8 b - -", {37,183,6559,23527}} );
	perft_data.push_back( {"r3k2r/8/8/8/3pPp2/8/8/R3K1RR b KQkq e3", {29,829,20501,624871,15446339,485647607}} );
	perft_data.push_back( {"8/7p/p5pb/4k3/P1pPn3/8/P5PP/1rB2RK1 b - d3", {5,117,3293,67197,1881089,38633283}} );
	perft_data.push_back( {"8/3K4/2p5/p2b2r1/5k2/8/8/1q6 b - -", {50,279,13310,54703,2538084,10809689,493407574}} );
	perft_data.push_back( {"rnbqkb1r/ppppp1pp/7n/4Pp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6", {31,570,17546,351806,11139762,244063299}} );
	perft_data.push_back( {"8/p7/8/1P6/K1k3p1/6P1/7P/8 w - -", {5,39,237,2002,14062,120995,966152,8103790}} );
	perft_data.push_back( {"n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - -", {24,496,9483,182838,3605103,71179139}} );
	perft_data.push_back( {"r3k2r/p6p/8/B7/1pp1p3/3b4/P6P/R3K2R w KQkq -", {17,341,6666,150072,3186478,77054993}} );
	perft_data.push_back( {"8/5p2/8/2k3P1/p3K3/8/1P6/8 b - -", {9,85,795,7658,72120,703851,6627106,64451405}} );
	perft_data.push_back( {"r3k2r/pb3p2/5npp/n2p4/1p1PPB2/6P1/P2N1PBP/R3K2R w KQkq -", {33,946,30962,899715,29179893}} );

	mutex m;
	perft_data_t::const_iterator it = perft_data.begin();
	std::vector<std::shared_ptr<thread>> threads;
	for( unsigned int i = 0; i < ctx.conf_.thread_count; ++i ) {
		auto t = std::make_shared<perft_thread>( perft_data, it, m );
		t->spawn();
		threads.push_back( t );
	}
	threads.clear();

	pass();
}

static void do_check_popcount( uint64_t v, uint64_t expected )
{
	uint64_t c = popcount(v);
	if( c != expected ) {
		std::cerr << "Popcount failed on " << v << ", got " << c << ", expected " << expected << std::endl;
		abort();
	}
}

static void check_popcount()
{
	checking("popcount");
	for( unsigned int i = 0; i < 64; ++i ) {
		uint64_t v = 1ull << i;
		do_check_popcount( v, 1 );

		do_check_popcount( v - 1, i );
	}
	pass();
}

static void do_check_bitscan( uint64_t v, uint64_t expected_count, uint64_t expected_sum, uint64_t expected_reverse_sum )
{
	uint64_t c = 0;
	uint64_t sum = 0;
	uint64_t v2 = v;

	while( v2 ) {
		++c;
		uint64_t i = bitscan_unset( v2 );
		sum += i * c;
	}
	if( c != expected_count || sum != expected_sum ) {
		std::cerr << "Bitscan failed on " << v << ", got " << c << ", " << sum << ", expected " << expected_count << ", " << expected_sum << std::endl;
		abort();
	}

	c = 0;
	sum = 0;
	v2 = v;
	while( v2 ) {
		++c;
		uint64_t i = bitscan_reverse( v2 );
		v2 ^= 1ull << i;
		sum += i * c;
	}
	if( c != expected_count || sum != expected_reverse_sum ) {
		std::cerr << "Reverse bitscan failed on " << v << ", got " << c << ", " << sum << ", expected " << expected_count << ", " << expected_reverse_sum << std::endl;
		abort();
	}
}

static void check_bitscan()
{
	checking("bitscan");
	do_check_bitscan( 0x5555555555555555ull, 32, 21824, 10912 );
	pass();
}

static void check_see( context& ctx, std::string const& fen, std::string const& ms, int expected )
{
	position p = test_parse_fen( ctx, fen );

	move m = test_parse_move( p, ms );
	
	short v = see( p, m );
	if( v > 0 && expected <= 0 ) {
		std::cerr << "See of " << fen << " " << ms << " is " << v << ", expected type " << expected << std::endl;
		abort();
	}
	if( v < 0 && expected >= 0 ) {
		std::cerr << "See of " << fen << " " << ms << " is " << v << ", expected type " << expected << std::endl;
		abort();
	}
	if( v == 0 && expected != 0 ) {
		std::cerr << "See of " << fen << " " << ms << " is " << v << ", expected type " << expected << std::endl;
		abort();
	}
}

static void check_see( context& ctx )
{
	checking("static exchange evaluation");

	check_see( ctx, "r3r3/p4ppp/4k3/R3n3/1N1KP3/8/6BP/8 w - -", "Rxe5", 1 );
	check_see( ctx, "8/1k6/3r4/8/3Pp3/8/6K1/3R4 b - d3 0 1", "exd", 1 );
	check_see( ctx, "8/1k6/3b4/8/3Pp3/8/6K1/3R4 b - d3 0 1", "exd", 0 );
	check_see( ctx, "1k1r4/1pp4p/p7/4p3/8/P5P1/1PP4P/2K1R3 w - -", "Rxe5", 1 );
	check_see( ctx, "1k1r3q/1ppn3p/p4b2/4p3/8/P2N2P1/1PP1R1BP/2K1Q3 w - -", "Nxe5", -1 );

	pass();
}


void do_check_disambiguation( context& ctx, std::string const& fen, std::string const& ms, std::string const& ref )
{
	position p = test_parse_fen( ctx, fen );

	move m;
	std::string error;
	if( !parse_move( p, ms, m, error ) ) {
		std::cerr << error << ": " << ms << std::endl;
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


void check_disambiguation( context& ctx )
{
	checking("disambiguations");
	do_check_disambiguation( ctx, "2K5/8/8/4N3/8/8/8/2k3N1 w - - 0 1", "Ngf3", "Ngf3" );
	do_check_disambiguation( ctx, "2K5/8/8/4N3/8/8/8/2k3N1 w - - 0 1", "N1f3", "Ngf3" );
	do_check_disambiguation( ctx, "2K5/8/8/4N3/8/8/8/2k1N3 w - - 0 1", "Ne1f3", "N1f3" );
	do_check_disambiguation( ctx, "2K5/8/8/4N3/8/8/8/2k1N3 w - - 0 1", "N1f3", "N1f3" );
	do_check_disambiguation( ctx, "2K5/8/8/4N3/8/8/8/2k1N1N1 w - - 0 1", "Ne1f3", "Ne1f3" );
	do_check_disambiguation( ctx, "2K5/8/8/4N3/8/8/8/2k1N1N1 w - - 0 1", "Ne5f3", "N5f3" );

	ctx.conf_.fischer_random = true;
	do_check_disambiguation( ctx, "rknbbnrq/pppppppp/8/8/8/8/PPPPPPPP/R1NBBK1R w H - 0 1", "f1g1", "Kg1" );
	do_check_disambiguation( ctx, "rknbbnrq/pppppppp/8/8/8/8/PPPPPPPP/R1NBBK1R w H - 0 1", "Kg1", "Kg1" );
	do_check_disambiguation( ctx, "rknbbnrq/pppppppp/8/8/8/8/PPPPPPPP/R1NBBK1R w H - 0 1", "O-O", "O-O" );
	do_check_disambiguation( ctx, "rknbbnrq/pppppppp/8/8/8/8/PPPPPPPP/R1NBBK1R w H - 0 1", "f1h1", "O-O" );
	do_check_disambiguation( ctx, "rknbbnrq/pppppppp/8/8/8/8/PPPPPPPP/R1NBBKR1 w G - 0 1", "f1g1", "O-O" );
	ctx.conf_.fischer_random = false;

	pass();
}


void check_condition_wait()
{
	checking("condition wait");
	mutex m;
	scoped_lock l(m);
	condition c;

	for( int i = 100; i <= 1000; i += 100 ) {
		duration wait = duration::milliseconds(i);
		timestamp const start;
		c.wait(l, wait);
		timestamp const stop;

		duration elapsed = stop - start;

		if( elapsed > (wait + duration::milliseconds(1)) || elapsed < wait ) {
			std::cerr << "Expected to wait: " << wait.milliseconds() << " ms" << std::endl;
			std::cerr << "Actually waited:  " << elapsed.milliseconds() << " ms" << std::endl;
#if UNIX
			if( elapsed < wait ) {
				std::cerr << "If there has been a leap second since last reboot, update your kernel and reboot!." << std::endl;
			}
#endif
			if( elapsed + duration::milliseconds(10) < wait ||
				elapsed > wait + duration::milliseconds(10) )
			{
				std::cerr << "Difference is more than 10ms" << std::endl;
				abort();
			}
		}
	}
	pass();
}


void check_kpvk( context& ctx, std::string const& fen, bool evaluated, int expected )
{
	position p = test_parse_fen( ctx, fen );

	short s;
	if( evaluate_endgame( p, s ) != evaluated ) {
		abort();
	}
	if( evaluated ) {
		if( !expected && s != result::draw ) {
			abort();
		}
		else if( expected > 0 && s < result::win_threshold ) {
			abort();
		}
		else if( expected < 0 && s >= result::loss_threshold ) {
			abort();
		}
	}

	position p2 = test_parse_fen( ctx, flip_fen( fen ) );
	expected *= -1;

	if( evaluate_endgame( p2, s ) != evaluated ) {
		abort();
	}

	if( evaluated ) {
		if( !expected && s != result::draw ) {
			abort();
		}
		else if( expected > 0 && s < result::win_threshold ) {
			abort();
		}
		else if( expected < 0 && s >= result::loss_threshold ) {
			abort();
		}
	}
}


void check_endgame_eval( context& ctx )
{
	checking("endgame evaluation");

	check_kpvk( ctx, "k7/8/7p/8/8/8/8/1K6 w - - 1 1", false, 0 );
	check_kpvk( ctx, "k7/8/7p/8/8/8/8/1K6 b - - 1 1", true, -1 );
	check_kpvk( ctx, "k7/7K/7p/8/8/8/8/8 b - - 1 1", true, -1 );
	check_kpvk( ctx, "k7/7K/7p/8/8/8/8/8 w - - 1 1", false, 0 );
	check_kpvk( ctx, "k7/8/2K5/7p/8/8/8/8 b - - 1 1", true, -1 );
	check_kpvk( ctx, "k7/8/2K5/7p/8/8/8/8 w - - 1 1", false, 0 );

	std::string const fens[] = {
		"8/3k4/8/8/5K2/8/8/8 w - - 0 1",
		"8/8/2k5/4n1K1/8/8/8/8 w - - 0 1",
		"8/6B1/8/8/8/7K/2k5/8 w - - 0 1",
		"5B2/8/8/2n5/8/8/2k4K/8 w - - 0 1",
		"5b2/8/8/2B5/8/8/2k4K/8 w - - 0 1",
		"4Q3/8/8/2q5/8/8/2k4K/8 w - - 0 1",
		"8/5K2/6R1/2r5/1k6/8/8/8 w - - 0 1",
		"8/5K2/6R1/8/1k6/8/3b4/8 w - - 0 1",
		"8/5K2/6R1/8/1k6/8/4n3/8 w - - 0 1",
		"r7/5K2/6R1/8/1k6/8/4n3/8 w - - 0 1",
		"r7/5K2/6R1/8/1k6/8/1b6/8 w - - 0 1",
		"3n4/K7/8/6Q1/4k3/8/8/5n2 w - - 0 1",
		"8/8/8/6Q1/K3k3/2q5/8/5b2 w - - 0 1",
		"8/8/8/6Q1/K3k3/8/7b/5b2 w - - 0 1",
		"8/8/8/6Q1/K3k3/8/7n/5q2 w - - 0 1",
		"8/8/3k4/6P1/8/2K5/7n/8 w - - 0 1",
		"1b6/8/8/6P1/4k3/2K5/8/8 w - - 0 1",
		"8/7k/8/8/8/7P/7K/8 w - - 0 1",
		"8/7k/8/8/8/1B5P/7K/8 w - - 0 1",
		"8/2b4k/8/8/3P4/3K4/2B5/8 w - - 0 1",
		"8/7k/8/8/3N4/3K4/2B5/8 w - - 0 1",
		"7k/8/8/1p6/8/8/8/6K1 b - - 0 1",
		"7k/8/8/1p6/8/8/8/7K w - - 0 1"
	};

	for( auto const& fen: fens ) {
		position p = test_parse_fen( ctx, fen );
		position p2 = test_parse_fen( ctx, flip_fen( fen ) );

		short ev = 0;
		if( !evaluate_endgame( p, ev ) ) {
			std::cerr << "Not recognized as endgame: " << fen << std::endl;
			abort();
		}
		short ev2 = 0;
		if( !evaluate_endgame( p2, ev2 ) ) {
			std::cerr << "Not recognized as endgame: " << flip_fen( fen ) << std::endl;
			abort();
		}

		if( ev != -ev2 ) {
			std::cerr << "Difference in endgame evaluation: " << ev << " " << ev2 << std::endl;
			std::cerr << "Fen: " << fen << std::endl;
			abort();
		}
	}
	pass();
}

void check_time()
{
	checking("time classes");
	timestamp t;
	timestamp t2;
	timestamp t3 = t2;
	millisleep(2);
	timestamp t4;

	if( t > t2 ) {
		std::cerr << "Timestamp error: t > t2" << std::endl;
		abort();
	}
	if( t2 != t3 ) {
		std::cerr << "Timestamp error: t2 != t3" << std::endl;
		abort();
	}
	if( t4 <= t3 ) {
		std::cerr << "Timestamp error: t4 <= t3" << std::endl;
		abort();
	}
	if( t3 >= t4 ) {
		std::cerr << "Timestamp error: t3 >= t4" << std::endl;
		abort();
	}


	duration d( duration::milliseconds(100) );

	if( d.seconds() != 0 ) {
		std::cerr << "duration::milliseconds(100).seconds() != 0" << std::endl;
		abort();
	}
	if( d.nanoseconds() != 100000000 ) {
		std::cerr << "duration::milliseconds(100).nanoseconds() != 100000000" << std::endl;
		abort();
	}
	pass();
}

void check_scale()
{
	checking("checking smoothness of game phase scaling");

	score v(20, 20);
	score v2(618, 598);
	for( int i = eval_values::phase_transition_material_begin; i >= eval_values::phase_transition_material_end; --i ) {
		short scaled = v.scale( i );

		if( scaled != 20 ) {
			std::cerr << "Scaling did not return correct value" << std::endl;
			abort();
		}

		short scaled2 = v2.scale( i );
		short nscaled2 = (-v2).scale( i );
		if( scaled2 != -nscaled2 ) {
			v2.scale(i);
			(-v2).scale(i);
			std::cerr << "Scaling of negated score did not return negated value" << std::endl;
			abort();
		}
	}

	pass();
}

class context_isolation_test_thread : public thread
{
public:
	context_isolation_test_thread()
		: nodes_()
	{
		ctx_.conf_.memory = 1;
		ctx_.conf_.thread_count = 1;
		ctx_.tt_.init( 1 );
		ctx_.pawn_tt_.init( 1 );
	}

	virtual ~context_isolation_test_thread()
	{
		join();
	}

	virtual void onRun()
	{
		calc_manager c(ctx_);

		position p;
		seen_positions seen;

		timestamp start;
		c.calc( p, 13, start, duration::infinity(), duration::infinity(), 0, seen, null_new_best_move_cb );

#if USE_STATISTICS
		statistics& s = c.stats();
		s.accumulate( duration( start, timestamp() ) );
		nodes_ = s.total_full_width_nodes + s.total_quiescence_nodes;
#endif
	}

	uint64_t nodes_;
	context ctx_;
};

void test_context_isolation()
{
	checking("context isolation");

	bool debug = logger::show_debug();
	logger::show_debug( false );

	std::array<context_isolation_test_thread, 10> threads;
	for( auto& t : threads ) {
		t.spawn();
	}
	for( auto& t : threads ) {
		t.join();
	}

	uint64_t nodes = threads[0].nodes_;
	for( auto& t : threads ) {
		if( t.nodes_ != nodes ) {
			std::cerr << "Contexts are not isolated, different node counts" << std::endl;
			abort();
		}
	}

	logger::show_debug( debug );

	pass();
}

void check_tt( context& ctx)
{
	checking("transposition table");

	if( hash::max_depth() < (MAX_DEPTH * DEPTH_FACTOR + MAX_QDEPTH ) ) {
		std::cerr << "Max depth cannot be stored in transition table" << std::endl;
		std::cerr << hash::max_depth() << " < " << (MAX_DEPTH * DEPTH_FACTOR + MAX_QDEPTH ) << std::endl;
		abort();
	}
	randgen rng;

	uint64_t hash = rng.get_uint64();
	

	short alpha = -123;
	short beta = 99;
	short eval = 556;
	short fev = -24;

	move bm;
	bm.d = 1234;
	ctx.tt_.store(hash, 23, 4, eval, alpha, beta, bm, 55, fev );

	{
		short eval2 = result::win;
		short fev2 = result::win;
		move bm2;

		score_type::type t = ctx.tt_.lookup( hash, 23, 4, alpha, beta, eval2, bm2, fev2 );

		if( t != score_type::lower_bound || eval2 != eval || bm2 != bm || fev2 != fev ) {
			std::cerr << "Looked up transposition table entry doesn't match stored one.\n";
			abort();
		}
	}

	beta = 1000;
	ctx.tt_.store(hash, 23, 4, eval, alpha, beta, move(), 55, fev );

	{
		short eval2 = result::win;
		short fev2 = result::win;
		move bm2;

		score_type::type t = ctx.tt_.lookup( hash, 23, 4, alpha, beta, eval2, bm2, fev2 );

		if( t != score_type::exact || eval2 != eval || bm2 != bm || fev2 != fev ) {
			std::cerr << "Looked up transposition table entry doesn't match stored one.\n";
			abort();
		}
	}

	eval = -5666;
	ctx.tt_.store(hash, 23, 4, eval, alpha, beta, move(), 55, fev );

	{
		short eval2 = result::win;
		short fev2 = result::win;
		move bm2;

		score_type::type t = ctx.tt_.lookup( hash, 23, 4, alpha, beta, eval2, bm2, fev2 );

		if( t != score_type::upper_bound || eval2 != eval || bm2 != bm || fev2 != fev ) {
			std::cerr << "Looked up transposition table entry doesn't match stored one.\n";
			abort();
		}
	}

	pass();
}

}

bool selftest()
{
	// Start by checking basics
	check_popcount();
	check_bitscan();
	check_time();

	context ctx;
	ctx.pawn_tt_.init( ctx.conf_.pawn_hash_table_size() );
	ctx.tt_.init( 1 );

	check_see( ctx );

	check_disambiguation( ctx );

	check_scale();
	check_eval();
	check_endgame_eval( ctx );

	check_tt( ctx );

	test_pst();
	test_incorrect_positions( ctx );
	process_test_positions( ctx );
	test_move_generation( ctx );
	test_move_legality_check( ctx );
	test_zobrist( ctx );
	test_lazy_eval( ctx );

	check_condition_wait();

	test_context_isolation();

	test_perft( ctx );

	std::cerr << "Self test passed" << std::endl;
	return true;
}
