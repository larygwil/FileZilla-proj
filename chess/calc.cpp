#include "chess.hpp"
#include "assert.hpp"
#include "calc.hpp"
#include "detect_check.hpp"
#include "hash.hpp"
#include "eval.hpp"
#include "util.hpp"
#include "platform.hpp"
#include "statistics.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <map>

int const MAX_DEPTH = 9;

unsigned long long hash_position( position_base const& p, color::type c ) {
	unsigned long long hash = 0;

	char const* it = reinterpret_cast<char const*>(&p);
	char const* end = it + sizeof( position_base );
	for( ; it != end; ++it ) {
		hash += *it;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);
	hash += (hash << 15);

//	hash = *reinterpret_cast<const unsigned long long*>(reinterpret_cast<const char*>(&p.pieces[0][0]));
//	hash >>= 8;
//	hash ^= *reinterpret_cast<const unsigned long long*>(reinterpret_cast<const char*>(&p.pieces[0][0]) + 8);
//	hash >>= 8;
//	hash ^= *reinterpret_cast<const unsigned long long*>(reinterpret_cast<const char*>(&p.pieces[0][0]) + 16);
//	hash >>= 8;
//	hash ^= *reinterpret_cast<const unsigned long long*>(reinterpret_cast<const char*>(&p.pieces[0][0]) + 24);
//	hash ^= p.promotions[0] << 13;
//	hash ^= p.promotions[1];

	if( c ) {
		hash = ~hash;
	}

	return hash;
}

// Adds the move if it does not result in a check
void add_if_legal( position const& p, color::type c, move_info*& moves, check_map const& check, unsigned char const& pi, unsigned char const& new_col, unsigned char const& new_row )
{
	piece const& pp = p.pieces[c][pi];
	unsigned char const& cv_old = check.board[pp.column][pp.row];
	unsigned char const& cv_new = check.board[new_col][new_row];
	if( check.check ) {
		if( cv_old ) {
			// Can't come to rescue, this piece is already blocking yet another check.
			return;
		}
		if( cv_new != check.check ) {
			// Target position does capture checking piece nor blocks check
			return;
		}
	}
	else {
		if( cv_old && cv_old != cv_new ) {
			return;
		}
	}

	move_info mi;
	mi.new_pos = p;

	mi.m.piece = pi;
	mi.m.target_col = new_col;
	mi.m.target_row = new_row;

	apply_move( mi.new_pos, mi.m, c );

	mi.evaluation = (evaluate( mi.new_pos, c ) << 6) | get_random_6bit();

	*(moves++) = mi;
}


void add_if_legal_king( position const& p, color::type c, move_info*& moves, unsigned char new_col, unsigned char new_row )
{
	move_info mi;
	mi.new_pos = p;

	mi.m.piece = pieces::king;
	mi.m.target_col = new_col;
	mi.m.target_row = new_row;

	apply_move( mi.new_pos, mi.m, c );

	if( detect_check( mi.new_pos, c ) ) {
		return;
	}

	mi.evaluation = (evaluate( mi.new_pos, c ) << 6) | get_random_6bit();

	*(moves++) = mi;
}


void calc_moves_king( position const& p, color::type c, move_info*& moves, check_map const& check, signed char cx, signed char cy )
{
	piece const& pp = p.pieces[c][pieces::king];
	if( cx < 0 && pp.column < 1 ) {
		return;
	}
	if( cy < 0 && pp.row < 1 ) {
		return;
	}
	if( cx > 0 && pp.column >= 7 ) {
		return;
	}
	if( cy > 0 && pp.row >= 7 ) {
		return;
	}

	unsigned char new_col = pp.column + cx;
	unsigned char new_row = pp.row + cy;

	piece const& other_king = p.pieces[1-c][pieces::king];
	int kx = static_cast<int>(new_col) - other_king.column;
	int ky = static_cast<int>(new_row) - other_king.row;
	if( kx <= 1 && kx >= -1 && ky <= 1 && ky >= -1 ) {
		// Other king too close
		return;
	}

	unsigned char target_piece = p.board[new_col][new_row];
	if( target_piece != pieces::nil ) {
		if( (target_piece >> 4) == c ) {
			return;
		}
	}

	add_if_legal_king( p, c, moves, new_col, new_row );
}

void calc_moves_king( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	piece const& pp = p.pieces[c][pieces::king];

	calc_moves_king( p, c, moves, check, 1, 0 );
	calc_moves_king( p, c, moves, check, 1, -1 );
	calc_moves_king( p, c, moves, check, 0, -1 );
	calc_moves_king( p, c, moves, check, -1, -1 );
	calc_moves_king( p, c, moves, check, -1, 0 );
	calc_moves_king( p, c, moves, check, -1, 1 );
	calc_moves_king( p, c, moves, check, 0, 1 );
	calc_moves_king( p, c, moves, check, 1, 1 );

	if( check.check ) {
		return;
	}

	// Queenside castling
	if( p.pieces[c][pieces::rook1].special ) {
		if( p.board[1][pp.row] == pieces::nil && p.board[2][pp.row] == pieces::nil && p.board[3][pp.row] == pieces::nil ) {
			if( !detect_check( p, c, 3, pp.row ) ) {
				add_if_legal_king( p, c, moves, 2, pp.row );
			}
		}
	}
	// Kingside castling
	if( p.pieces[c][pieces::rook2].special ) {
		if( p.board[5][pp.row] == pieces::nil && p.board[6][pp.row] == pieces::nil ) {
			if( !detect_check( p, c, 5, pp.row ) ) {
				add_if_legal_king( p, c, moves, 6, pp.row );
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; ++cx ) {
		for( int cy = -1; cy <= 1; ++cy ) {
			if( !cx && !cy ) {
				continue;
			}

			int x, y;
			for( x = pp.column + cx, y = pp.row + cy; x >= 0 && x <= 7 && y >= 0 && y <= 7; x += cx, y += cy ) {
				unsigned char target = p.board[x][y];
				if( target == pieces::nil ) {
					add_if_legal( p, c, moves, check, pi, x, y );
				}
				else {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, moves, check, pi, x, y );
					}
					break;
				}
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	piece const& pp = p.pieces[c][pieces::queen];
	if( pp.alive ) {
		calc_moves_queen( p, c, moves, check, pieces::queen, pp );
	}
}

void calc_moves_bishop( position const& p, color::type c, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int cy = -1; cy <= 1; cy += 2 ) {
			int x, y;
			for( x = pp.column + cx, y = pp.row + cy; x >= 0 && x <= 7 && y >= 0 && y <= 7; x += cx, y += cy ) {
				unsigned char target = p.board[x][y];
				if( target == pieces::nil ) {
					add_if_legal( p, c, moves, check, pi, x, y );
				}
				else {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, moves, check, pi, x, y );
					}
					break;
				}
			}
		}
	}
}


void calc_moves_bishops( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	{
		piece const& pp = p.pieces[c][pieces::bishop1];
		if( pp.alive ) {
			calc_moves_bishop( p, c, moves, check, pieces::bishop1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::bishop2];
		if( pp.alive ) {
			calc_moves_bishop( p, c, moves, check, pieces::bishop2, pp );
		}
	}
}


void calc_moves_rook( position const& p, color::type c, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int x = pp.column + cx; x >= 0 && x <= 7; x += cx ) {
			unsigned char target = p.board[x][pp.row];
			if( target == pieces::nil ) {
				add_if_legal( p, c, moves, check, pi, x, pp.row );
			}
			else {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, moves, check, pi, x, pp.row );
				}
				break;
			}
		}
	}
	for( int cy = -1; cy <= 1; cy += 2 ) {
		for( int y = pp.row + cy; y >= 0 && y <= 7; y += cy ) {
			unsigned char target = p.board[pp.column][y];
			if( target == pieces::nil ) {
				add_if_legal( p, c, moves, check, pi, pp.column, y );
			}
			else {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, moves, check, pi, pp.column, y );
				}
				break;
			}
		}
	}
}


void calc_moves_rooks( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	{
		piece const& pp = p.pieces[c][pieces::rook1];
		if( pp.alive ) {
			calc_moves_rook( p, c, moves, check, pieces::rook1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::rook2];
		if( pp.alive ) {
			calc_moves_rook( p, c, moves, check, pieces::rook2, pp );
		}
	}
}


void calc_moves_knight( position const& p, color::type c, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp, int cx, int cy )
{
	int new_column = cx + pp.column;

	if( new_column < 0 || new_column > 7 ) {
		return;
	}

	int new_row = cy + pp.row;
	if( new_row < 0 || new_row > 7 ) {
		return;
	}

	int new_target = p.board[new_column][new_row];
	if( new_target != pieces::nil ) {
		if( (new_target >> 4) == c ) {
			return;
		}
	}

	add_if_legal( p, c, moves, check, pi, new_column, new_row );
}


void calc_moves_knight( position const& p, color::type c, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
{
	calc_moves_knight( p, c, moves, check, pi, pp, 2, 1 );
	calc_moves_knight( p, c, moves, check, pi, pp, 2, -1 );
	calc_moves_knight( p, c, moves, check, pi, pp, 1, -2 );
	calc_moves_knight( p, c, moves, check, pi, pp, -1, -2 );
	calc_moves_knight( p, c, moves, check, pi, pp, -2, -1 );
	calc_moves_knight( p, c, moves, check, pi, pp, -2, 1 );
	calc_moves_knight( p, c, moves, check, pi, pp, -1, 2 );
	calc_moves_knight( p, c, moves, check, pi, pp, 1, 2 );
}


void calc_moves_knights( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	{
		piece const& pp = p.pieces[c][pieces::knight1];
		if( pp.alive ) {
			calc_moves_knight( p, c, moves, check, pieces::knight1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::knight2];
		if( pp.alive ) {
			calc_moves_knight( p, c, moves, check, pieces::knight2, pp );
		}
	}
}

void calc_diagonal_pawn_move( position const& p, color::type c, move_info*& moves, check_map const& check, unsigned int pi, piece const& pp, unsigned char new_col, unsigned char new_row )
{
	unsigned char target = p.board[new_col][new_row];
	if( target == pieces::nil ) {
		if( p.can_en_passant != pieces::nil ) {
			piece const& ep = p.pieces[1-c][p.can_en_passant];
			ASSERT( ep.alive );
			if( ep.column == new_col && ep.row == pp.row ) {

				// Capture en-passant

				// TODO: Special case: black queen, black pawn, white pawn, white king from left to right on rank 5. Capturing opens up check!
				add_if_legal( p, c, moves, check, pi, new_col, new_row );
			}
		}
	}
	else if( (target >> 4) != c ) {
		// Capture!
		add_if_legal( p, c, moves, check, pi, new_col, new_row );
	}
}

void calc_moves_pawns( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	for( unsigned int pi = pieces::pawn1; pi <= pieces::pawn8; ++pi ) {
		piece const& pp = p.pieces[c][pi];
		if( pp.alive ) {
			if( !pp.special ) {
				unsigned char new_row = (c == color::white) ? (pp.row + 1) : (pp.row - 1);
				unsigned char target = p.board[pp.column][new_row];

				if( pp.column > 0 ) {
					unsigned char new_col = pp.column - 1;
					calc_diagonal_pawn_move( p, c, moves, check, pi, pp, new_col, new_row );
				}
				if( pp.column < 7 ) {
					unsigned char new_col = pp.column + 1;
					calc_diagonal_pawn_move( p, c, moves, check, pi, pp, new_col, new_row );
				}

				if( target == pieces::nil ) {

					add_if_legal( p, c, moves, check, pi, pp.column, new_row );

					if( pp.row == ( (c == color::white) ? 1 : 6) ) {
						// Moving two rows from starting row
						new_row = (c == color::white) ? (pp.row + 2) : (pp.row - 2);

						unsigned char target = p.board[pp.column][new_row];
						if( target == pieces::nil ) {
							add_if_legal( p, c, moves, check, pi, pp.column, new_row );
						}
					}
				}
			}
			else {
				// Promoted piece
				unsigned char promoted = (p.promotions[c] >> (2 * (pi - pieces::pawn1) ) ) & 0x03;
				if( promoted == promotions::queen ) {
					calc_moves_queen( p, c, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else if( promoted == promotions::rook ) {
					calc_moves_rook( p, c, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else if( promoted == promotions::bishop ) {
					calc_moves_bishop( p, c, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else {//if( promoted == promotions::knight ) {
					calc_moves_knight( p, c, moves, check, static_cast<pieces::type>(pi), pp );
				}
			}
		}
	}
}

struct MoveSort {
	bool operator()( move_info const& lhs, move_info const& rhs ) const {
		return lhs.evaluation > rhs.evaluation;
	}
} moveSort;

void calculate_moves( position const& p, color::type c, move_info*& moves, check_map const& check )
{
	move_info* start = moves;

	calc_moves_king( p, c, moves, check );

	if( !check.check || !check.multiple() )
	{
		calc_moves_pawns( p, c, moves, check );
		calc_moves_queen( p, c, moves, check );
		calc_moves_rooks( p, c, moves, check );
		calc_moves_bishops( p, c, moves, check );
		calc_moves_knights( p, c, moves, check );
	}

	std::sort( start, moves, moveSort );

	while( start != moves ) {
		start->evaluation >>= 6;
		++start;
	}
}


int step( int depth, int const max_depth, position const& p, int current_evaluation, bool captured, color::type c, int alpha, int beta )
{
#if USE_QUIESCENCE
	int limit;
	if( captured ) {
		limit = max_depth + USE_QUIESCENCE;
	}
	else {
		limit = max_depth;
	}
#else
	int const limit = max_depth;
#endif

	unsigned long long hash = hash_position( p, c );

	bool got_old_best = false;
	step_data d;

	if( hash && lookup( hash, reinterpret_cast<unsigned char * const>(&d) ) ) {
		if( d.terminal ) {
			if( d.evaluation == result::loss ) {
				return result::loss + depth;
			}
			else if( d.evaluation == result::win ) {
				return result::win - depth;
			}
			else {
				return result::draw;
			}
		}
		else if( d.evaluation < result::loss_threshold ) {
			return d.evaluation + depth;
		}
		else if( d.evaluation > result::win_threshold ) {
			return d.evaluation - depth;
		}
		else if( (limit - depth) <= d.remaining_depth && alpha >= d.alpha && beta <= d.beta ) {
			return d.evaluation	;
		}
		//got_old_best = true;

		// TODO: Does this have to be cached?
	}
	else {
		calc_check_map( p, c, d.check );
	}

#if USE_QUIESCENCE
	if( depth >= limit && !d.check.check )
#else
	if( depth >= max_depth && !d.check.check )
#endif
	{
#ifdef USE_STATISTICS
		++stats.evaluated_leaves;
#endif
		return current_evaluation;
	}

	int best_value = result::loss;

	d.beta = beta;
	d.alpha = alpha;
	d.remaining_depth = limit - depth;

	if( got_old_best ) {
		// Not a terminal node, do this check early:
		if( depth >= limit ) {
	#ifdef USE_STATISTICS
			++stats.evaluated_leaves;
	#endif
			return current_evaluation;
		}

		int value = -step( depth + 1, max_depth, d.best_move.new_pos, -d.best_move.evaluation, d.best_move.captured, static_cast<color::type>(1-c), -beta, -alpha );
		if( value > best_value ) {

			best_value = value;

			if( value > alpha ) {
				alpha = value;
			}
			if( alpha >= beta ) {
#ifdef USE_STATISTICS
				++stats.evaluated_intermediate;
#endif
				d.evaluation = alpha;
				store( hash, reinterpret_cast<unsigned char const* const>(&d) );
				return alpha;
			}
		}
	}


	move_info moves[200];
	move_info* pm = moves;
	calculate_moves( p, c, pm, d.check );

	if( pm == moves ) {
#if USE_TRANSPOSITION
		ASSERT( !got_old_best || d.terminal );
#endif
		if( d.check.check ) {
#if USE_TRANSPOSITION
			d.terminal = true;
			d.evaluation = result::loss;
			store( hash, reinterpret_cast<unsigned char const* const>(&d) );
#endif
#ifdef USE_STATISTICS
			++stats.evaluated_leaves;
#endif
			return result::loss + depth;
		}
		else {
#if USE_TRANSPOSITION
			d.terminal = true;
			d.evaluation = result::draw;
			store( hash, reinterpret_cast<unsigned char const* const>(&d) );
#ifdef USE_STATISTICS
			++stats.evaluated_leaves;
#endif
#endif
			return result::draw;
		}
	}
#if USE_TRANSPOSITION
	else {
		d.terminal = false;
	}
#endif

	if( depth >= limit ) {
#ifdef USE_STATISTICS
		++stats.evaluated_leaves;
#endif
		return current_evaluation;
	}

	
#ifdef USE_STATISTICS
	++stats.evaluated_intermediate;
#endif

	++depth;
	
	d.best_move = *moves;

	for( move_info const* it  = moves; it != pm; ++it ) {
		int value = -step( depth, max_depth, it->new_pos, -it->evaluation, it->captured, static_cast<color::type>(1-c), -beta, -alpha );
		if( value > best_value ) {
			best_value = value;

#if USE_TRANSPOSITION
			d.best_move = *it;
#endif

			if( value > alpha ) {
				alpha = value;
			}
		}
		if( alpha >= beta )
			break;
	}

#if USE_TRANSPOSITION
	d.evaluation = alpha;
	store( hash, reinterpret_cast<unsigned char const* const>(&d) );
#endif

	return alpha;
}



bool calc( position& p, color::type c, move& m, int& res )
{
	check_map check;
	calc_check_map( p, c, check );

	move_info moves[200];
	move_info* pm = moves;
	calculate_moves( p, c, pm, check );

	if( moves == pm ) {
		if( check.check ) {
			if( c == color::white ) {
				std::cerr << "BLACK WINS" << std::endl;
				res = result::loss;
			}
			else {
				std::cerr << std::endl << "WHITE WINS" << std::endl;
				res = result::win;
			}
			return false;
		}
		else {
			if( c == color::black ) {
				std::cout << std::endl;
			}
			std::cerr << "DRAW" << std::endl;
			res = result::draw;
			return false;
		}
	}

//	{
//		std::cerr << "Possible moves:";
//		move* mbegin = &moves[0];
//		move* mend = mbegin + count;
//		for( ; mbegin != mend; ++mbegin ) { // Optimize this, compiler!
//			std::cerr << " " << move_to_string( p, c, *mbegin );
//		}
//		std::cerr << std::endl;
//	}

	int const& count = pm - moves;

#if USE_TRANSPOSITION
	// Go through them sorted by previous evaluation. This way, if on time pressure,
	// we can abort processing at high depths early if needed.
	typedef std::multimap<int, move_info> sorted_moves;
	sorted_moves old_sorted;
	for( move_info const* it  = moves; it != pm; ++it ) {
		old_sorted.insert( std::make_pair( it - moves, *it ) );
	}

	unsigned long long start = get_time();

	for( int max_depth = 1; max_depth <= MAX_DEPTH; max_depth += 2 )
	{
#else
	{
		int max_depth = MAX_DEPTH;
#endif

		int alpha = result::loss;
		int beta = result::win;

		sorted_moves sorted;

		int i;
		sorted_moves::const_iterator it;
		bool abort = false;
		for( i = 0, it = old_sorted.begin(); it != old_sorted.end(); ++it, ++i ) {

			unsigned long long now = get_time();
			if( (now - start) > 30000 ) {
				std::cerr << "Aborting search due to time limit at depth " << max_depth << " with " << i << " of " << count << " moves evaluated." << std::endl;
				abort = true;
				break;
			}

			int value = -step( 1, max_depth, it->second.new_pos, -it->second.evaluation, it->second.captured, static_cast<color::type>(1-c), -beta, -alpha );

			if( value > alpha ) {
				alpha = value;
			}

			sorted.insert( std::make_pair( -value, it->second ) );
		}

		res = alpha;
#if USE_TRANSPOSITION
		if( alpha < result::loss_threshold || alpha > result::win_threshold ) {
			if( max_depth < MAX_DEPTH ) {
				std::cerr << "Early break at " << max_depth << std::endl;
			}
			abort = true;
		}
#endif

		if( !sorted.empty() ) {
			sorted.swap( old_sorted );
		}
		if( abort ) {
			break;
		}
	}

	m = old_sorted.begin()->second.m;

	unsigned long long stop = get_time();
	print_stats( start, stop );
	reset_stats();

	return true;
}


