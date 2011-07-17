#include "chess.hpp"
#include "assert.hpp"
#include "calc.hpp"
#include "detect_check.hpp"
#include "hash.hpp"
#include "eval.hpp"
#include "util.hpp"
#include "platform.hpp"
#include "statistics.hpp"
#include "zobrist.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <map>

int const MAX_DEPTH = 9;

// Adds the move if it does not result in a check
void add_if_legal( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, unsigned char const& pi, unsigned char const& new_col, unsigned char const& new_row )
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

	mi.m.piece = pi;
	mi.m.target_col = new_col;
	mi.m.target_row = new_row;

	mi.evaluation = evaluate_move( p, c, current_evaluation, mi.m );
	mi.random = get_random_unsigned_char();

	*(moves++) = mi;
}


void add_if_legal_king( position const& p, color::type c, int const /*current_evaluation*/,  move_info*& moves, unsigned char new_col, unsigned char new_row )
{
	move_info mi;


	mi.m.piece = pieces::king;
	mi.m.target_col = new_col;
	mi.m.target_row = new_row;

	position new_pos = p;
	apply_move( new_pos, mi.m, c );

	if( detect_check( new_pos, c ) ) {
		return;
	}

	mi.evaluation = evaluate( new_pos, c );
	mi.random = get_random_unsigned_char();

	*(moves++) = mi;
}


void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, signed char cx, signed char cy )
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

	add_if_legal_king( p, c, current_evaluation, moves, new_col, new_row );
}

void calc_moves_king( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	piece const& pp = p.pieces[c][pieces::king];

	calc_moves_king( p, c, current_evaluation, moves, check, 1, 0 );
	calc_moves_king( p, c, current_evaluation, moves, check, 1, -1 );
	calc_moves_king( p, c, current_evaluation, moves, check, 0, -1 );
	calc_moves_king( p, c, current_evaluation, moves, check, -1, -1 );
	calc_moves_king( p, c, current_evaluation, moves, check, -1, 0 );
	calc_moves_king( p, c, current_evaluation, moves, check, -1, 1 );
	calc_moves_king( p, c, current_evaluation, moves, check, 0, 1 );
	calc_moves_king( p, c, current_evaluation, moves, check, 1, 1 );

	if( check.check ) {
		return;
	}

	// Queenside castling
	if( p.pieces[c][pieces::rook1].special ) {
		if( p.board[1][pp.row] == pieces::nil && p.board[2][pp.row] == pieces::nil && p.board[3][pp.row] == pieces::nil ) {
			if( !detect_check( p, c, 3, pp.row ) ) {
				add_if_legal_king( p, c, current_evaluation, moves, 2, pp.row );
			}
		}
	}
	// Kingside castling
	if( p.pieces[c][pieces::rook2].special ) {
		if( p.board[5][pp.row] == pieces::nil && p.board[6][pp.row] == pieces::nil ) {
			if( !detect_check( p, c, 5, pp.row ) ) {
				add_if_legal_king( p, c, current_evaluation, moves, 6, pp.row );
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
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
					add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
				}
				else {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
					}
					break;
				}
			}
		}
	}
}


void calc_moves_queen( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	piece const& pp = p.pieces[c][pieces::queen];
	if( pp.alive ) {
		calc_moves_queen( p, c, current_evaluation, moves, check, pieces::queen, pp );
	}
}

void calc_moves_bishop( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int cy = -1; cy <= 1; cy += 2 ) {
			int x, y;
			for( x = pp.column + cx, y = pp.row + cy; x >= 0 && x <= 7 && y >= 0 && y <= 7; x += cx, y += cy ) {
				unsigned char target = p.board[x][y];
				if( target == pieces::nil ) {
					add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
				}
				else {
					if( (target >> 4) != c ) {
						add_if_legal( p, c, current_evaluation, moves, check, pi, x, y );
					}
					break;
				}
			}
		}
	}
}


void calc_moves_bishops( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	{
		piece const& pp = p.pieces[c][pieces::bishop1];
		if( pp.alive ) {
			calc_moves_bishop( p, c, current_evaluation, moves, check, pieces::bishop1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::bishop2];
		if( pp.alive ) {
			calc_moves_bishop( p, c, current_evaluation, moves, check, pieces::bishop2, pp );
		}
	}
}


void calc_moves_rook( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
{
	for( int cx = -1; cx <= 1; cx += 2 ) {
		for( int x = pp.column + cx; x >= 0 && x <= 7; x += cx ) {
			unsigned char target = p.board[x][pp.row];
			if( target == pieces::nil ) {
				add_if_legal( p, c, current_evaluation, moves, check, pi, x, pp.row );
			}
			else {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, current_evaluation, moves, check, pi, x, pp.row );
				}
				break;
			}
		}
	}
	for( int cy = -1; cy <= 1; cy += 2 ) {
		for( int y = pp.row + cy; y >= 0 && y <= 7; y += cy ) {
			unsigned char target = p.board[pp.column][y];
			if( target == pieces::nil ) {
				add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, y );
			}
			else {
				if( (target >> 4) != c ) {
					add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, y );
				}
				break;
			}
		}
	}
}


void calc_moves_rooks( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	{
		piece const& pp = p.pieces[c][pieces::rook1];
		if( pp.alive ) {
			calc_moves_rook( p, c, current_evaluation, moves, check, pieces::rook1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::rook2];
		if( pp.alive ) {
			calc_moves_rook( p, c, current_evaluation, moves, check, pieces::rook2, pp );
		}
	}
}


void calc_moves_knight( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp, int cx, int cy )
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

	add_if_legal( p, c, current_evaluation, moves, check, pi, new_column, new_row );
}


void calc_moves_knight( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, pieces::type pi, piece const& pp )
{
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, 2, 1 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, 2, -1 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, 1, -2 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, -1, -2 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, -2, -1 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, -2, 1 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, -1, 2 );
	calc_moves_knight( p, c, current_evaluation, moves, check, pi, pp, 1, 2 );
}


void calc_moves_knights( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	{
		piece const& pp = p.pieces[c][pieces::knight1];
		if( pp.alive ) {
			calc_moves_knight( p, c, current_evaluation, moves, check, pieces::knight1, pp );
		}
	}
	{
		piece const& pp = p.pieces[c][pieces::knight2];
		if( pp.alive ) {
			calc_moves_knight( p, c, current_evaluation, moves, check, pieces::knight2, pp );
		}
	}
}

void calc_diagonal_pawn_move( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check, unsigned int pi, piece const& pp, unsigned char new_col, unsigned char new_row )
{
	unsigned char target = p.board[new_col][new_row];
	if( target == pieces::nil ) {
		if( p.can_en_passant != pieces::nil ) {
			piece const& ep = p.pieces[1-c][p.can_en_passant];
			ASSERT( ep.alive );
			if( ep.column == new_col && ep.row == pp.row ) {

				// Capture en-passant

				// Special case: black queen, black pawn, white pawn, white king from left to right on rank 5. Capturing opens up check!
				piece const& kp = p.pieces[c][pieces::king];
				if( kp.row == pp.row ) {
					signed char cx = static_cast<signed char>(pp.column) - kp.column;
					if( cx > 0 ) {
						cx = 1;
					}
					else {
						cx = -1;
					}
					for( signed char col = pp.column + cx; col < 8 && col>= 0; col += cx ) {
						if( col == new_col ) {
							continue;
						}
						unsigned char t = p.board[col][pp.row];
						if( t == pieces::nil ) {
							continue;
						}
						if( ( t >> 4) == c ) {
							// Own piece
							break;
						}
						t &= 0x0f;
						if( t == pieces::queen || t == pieces::rook1 || t == pieces::rook2 ) {
							// Not a legal move unfortunately
							std::cerr << "ENPASSANT SPECIAL" << std::endl;
							return;
						}
						else if( t >= pieces::pawn1 && t <= pieces::pawn8 ) {
							piece const& pawn = p.pieces[1-c][t];
							if( !pawn.special ) {
								// Harmless pawn
							}

							unsigned short promoted = (p.promotions[1-c] >> (2 * (t - pieces::pawn1) ) ) & 0x03;
							if( promoted == promotions::queen || promoted == promotions::rook ) {
								// Not a legal move unfortunately
								std::cerr << "ENPASSANT SPECIAL" << std::endl;
								return;
							}
						}

						// Harmless piece
						break;
					}
				}
				add_if_legal( p, c, current_evaluation, moves, check, pi, new_col, new_row );
			}
		}
	}
	else if( (target >> 4) != c ) {
		// Capture!
		add_if_legal( p, c, current_evaluation, moves, check, pi, new_col, new_row );
	}
}

void calc_moves_pawns( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	for( unsigned int pi = pieces::pawn1; pi <= pieces::pawn8; ++pi ) {
		piece const& pp = p.pieces[c][pi];
		if( pp.alive ) {
			if( !pp.special ) {
				unsigned char new_row = (c == color::white) ? (pp.row + 1) : (pp.row - 1);
				unsigned char target = p.board[pp.column][new_row];

				if( pp.column > 0 ) {
					unsigned char new_col = pp.column - 1;
					calc_diagonal_pawn_move( p, c, current_evaluation, moves, check, pi, pp, new_col, new_row );
				}
				if( pp.column < 7 ) {
					unsigned char new_col = pp.column + 1;
					calc_diagonal_pawn_move( p, c, current_evaluation, moves, check, pi, pp, new_col, new_row );
				}

				if( target == pieces::nil ) {

					add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, new_row );

					if( pp.row == ( (c == color::white) ? 1 : 6) ) {
						// Moving two rows from starting row
						new_row = (c == color::white) ? (pp.row + 2) : (pp.row - 2);

						unsigned char target = p.board[pp.column][new_row];
						if( target == pieces::nil ) {
							add_if_legal( p, c, current_evaluation, moves, check, pi, pp.column, new_row );
						}
					}
				}
			}
			else {
				// Promoted piece
				unsigned char promoted = (p.promotions[c] >> (2 * (pi - pieces::pawn1) ) ) & 0x03;
				if( promoted == promotions::queen ) {
					calc_moves_queen( p, c, current_evaluation, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else if( promoted == promotions::rook ) {
					calc_moves_rook( p, c, current_evaluation, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else if( promoted == promotions::bishop ) {
					calc_moves_bishop( p, c, current_evaluation, moves, check, static_cast<pieces::type>(pi), pp );
				}
				else {//if( promoted == promotions::knight ) {
					calc_moves_knight( p, c, current_evaluation, moves, check, static_cast<pieces::type>(pi), pp );
				}
			}
		}
	}
}

struct MoveSort {
	bool operator()( move_info const& lhs, move_info const& rhs ) const {
		if( lhs.evaluation > rhs.evaluation ) {
			return true;
		}
		if( lhs.evaluation < rhs.evaluation ) {
			return false;
		}

		return lhs.random > rhs.random;
	}
} moveSort;

void calculate_moves( position const& p, color::type c, int const current_evaluation, move_info*& moves, check_map const& check )
{
	move_info* start = moves;

	calc_moves_king( p, c, current_evaluation, moves, check );

	if( !check.check || !check.multiple() )
	{
		calc_moves_pawns( p, c, current_evaluation, moves, check );
		calc_moves_queen( p, c, current_evaluation, moves, check );
		calc_moves_rooks( p, c, current_evaluation, moves, check );
		calc_moves_bishops( p, c, current_evaluation, moves, check );
		calc_moves_knights( p, c, current_evaluation, moves, check );
	}

	std::sort( start, moves, moveSort );
}


short step( int depth, int const max_depth, position const& p, unsigned long long hash, int current_evaluation, bool captured, color::type c, short alpha, short beta )
{
#if USE_QUIESCENCE
	int limit = max_depth;
	if( depth >= max_depth && captured ) {
		limit = max_depth + 1;
	}
#else
	int const limit = max_depth;
#endif

	bool got_old_best = false;
	step_data d;

	if( hash && lookup( hash, reinterpret_cast<unsigned char * const>(&d) ) ) {
		if( d.remaining_depth == -127 ) {
			if( d.evaluation == result::loss ) {
#if USE_STATISTICS
				++stats.transposition_table_cutoffs;
#endif
				return result::loss + depth;
			}
			else if( d.evaluation == result::win ) {
#if USE_STATISTICS
				++stats.transposition_table_cutoffs;
#endif
				return result::win - depth;
			}
			else {
#if USE_STATISTICS
				++stats.transposition_table_cutoffs;
#endif
				return result::draw;
			}
		}
//		else if( d.evaluation < result::loss_threshold ) {
//#if USE_STATISTICS
//			++stats.transposition_table_cutoffs;
//#endif
//			return result::loss + depth + 1;
//		}
//		else if( d.evaluation > result::win_threshold ) {
//#if USE_STATISTICS
//			++stats.transposition_table_cutoffs;
//#endif
//			return result::win - depth - 1;
//		}
		else if( (limit - depth) <= d.remaining_depth && alpha >= d.alpha && beta <= d.beta ) {
#if USE_STATISTICS
			++stats.transposition_table_cutoffs;
#endif
			return d.evaluation	;
		}
		got_old_best = true;

		// TODO: Does this have to be cached?
	}

	check_map check;
	calc_check_map( p, c, check );

#if USE_QUIESCENCE
	if( depth >= limit && !check.check )
#else
	if( depth >= max_depth && !check.check )
#endif
	{
#ifdef USE_STATISTICS
		++stats.evaluated_leaves;
#endif
		return current_evaluation;
	}

	short best_value = result::loss;

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

		position new_pos = p;
		bool captured = apply_move( new_pos, d.best_move.m, c );
		unsigned long long new_hash = update_zobrist_hash( p, c, hash, d.best_move.m );
		short value = -step( depth + 1, max_depth, new_pos, new_hash, -d.best_move.evaluation, captured, static_cast<color::type>(1-c), -beta, -alpha );
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
	calculate_moves( p, c, current_evaluation, pm, check );

	if( pm == moves ) {
		ASSERT( !got_old_best || d.remaining_depth == -127 );
		if( check.check ) {
			d.remaining_depth = -127;
			d.evaluation = result::loss;
			store( hash, reinterpret_cast<unsigned char const* const>(&d) );
#ifdef USE_STATISTICS
			++stats.evaluated_leaves;
#endif
			return result::loss + depth;
		}
		else {
			d.remaining_depth = -127;
			d.evaluation = result::draw;
			store( hash, reinterpret_cast<unsigned char const* const>(&d) );
#ifdef USE_STATISTICS
			++stats.evaluated_leaves;
#endif
			return result::draw;
		}
	}

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
		position new_pos = p;
		bool captured = apply_move( new_pos, it->m, c );
		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->m );
		int value = -step( depth, max_depth, new_pos, new_hash, -it->evaluation, captured, static_cast<color::type>(1-c), -beta, -alpha );
		if( value > best_value ) {
			best_value = value;

			d.best_move = *it;

			if( value > alpha ) {
				alpha = value;
			}
		}
		if( alpha >= beta )
			break;
	}

	d.evaluation = alpha;
	store( hash, reinterpret_cast<unsigned char const* const>(&d) );

	return alpha;
}


class processing_thread : public thread
{
public:
	processing_thread( mutex& mtx, condition& cond )
		: mutex_(mtx), cond_(cond), finished_()
	{
	}

	// Call locked
	bool finished() {
		return finished_;
	}

	// Call locked
	short result() {
		return result_;
	}

	void process( position const& p, color::type c, move_info const& m, short max_depth, short alpha, short beta )
	{
		p_ = p;
		c_ = c;
		m_ = m;
		max_depth_ = max_depth;
		alpha_ = alpha;
		beta_ = beta;
		finished_ = false;

		spawn();
	}

	move_info get_move() const { return m_; }

	virtual void onRun();
private:
	mutex& mutex_;
	condition& cond_;

	position p_;
	color::type c_;
	move_info m_;
	short max_depth_;
	short alpha_;
	short beta_;

	bool finished_;
	short result_;
};


void processing_thread::onRun()
{
	position new_pos = p_;
	bool captured = apply_move( new_pos, m_.m, c_ );
	unsigned long long hash = get_zobrist_hash( new_pos, static_cast<color::type>(1-c_) );
	short value = -step( 1, max_depth_, new_pos, hash, -m_.evaluation, captured, static_cast<color::type>(1-c_), -beta_, -alpha_ );

	scoped_lock l( mutex_ );
	result_ = value;
	finished_ = true;
	cond_.signal( l );
}

bool calc( position& p, color::type c, move& m, int& res )
{
	check_map check;
	calc_check_map( p, c, check );

	move_info moves[200];
	move_info* pm = moves;
	int current_evaluation = evaluate( p, c );
	calculate_moves( p, c, current_evaluation, pm, check );

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

	// Go through them sorted by previous evaluation. This way, if on time pressure,
	// we can abort processing at high depths early if needed.
	typedef std::multimap<short, move_info> sorted_moves;
	sorted_moves old_sorted;
	for( move_info const* it  = moves; it != pm; ++it ) {
		old_sorted.insert( std::make_pair( it - moves, *it ) );
	}

	unsigned long long start = get_time();

	mutex mtx;
	init_mutex( mtx );

	condition cond;

	std::vector<processing_thread*> threads;
	int thread_count = 6;
	for( int t = 0; t < thread_count; ++t ) {
		threads.push_back( new processing_thread( mtx, cond ) );
	}

	for( int max_depth = 1; max_depth <= MAX_DEPTH; max_depth += 2 )
	{
		short alpha = result::loss;
		short beta = result::win;

		sorted_moves sorted;

		sorted_moves::const_iterator it = old_sorted.begin();
		int evaluated = 0;

		bool abort = false;
		bool done = false;
		while( !done ) {
			unsigned long long now = get_time();
			if( (now - start) > 30000 ) {
				std::cerr << "Triggering search abort due to time limit at depth " << max_depth << std::endl;
				abort = true;
				break;
			}

			while( it != old_sorted.end() && !abort ) {
				int t;
				for( t = 0; t < thread_count; ++t ) {
					if( threads[t]->spawned() ) {
						continue;
					}

					threads[t]->process( p, c, it->second, max_depth, alpha, beta );

					// First one is run on its own to get a somewhat sane lower bound for others to work with.
					if( it++ == old_sorted.begin() ) {
						goto break2;
					}
					break;
				}

				if( t == thread_count ) {
					break;
				}
			}
break2:

			scoped_lock l( mtx );
			cond.wait( l );

			bool all_idle = true;
			for( int t = 0; t < thread_count; ++t ) {
				if( !threads[t]->spawned() ) {
					continue;
				}

				if( !threads[t]->finished() ) {
					all_idle = false;
					continue;
				}

				threads[t]->join();
				int value = threads[t]->result();
				++evaluated;
				if( value > alpha ) {
					alpha = value;
				}

				sorted.insert( std::make_pair( -value, threads[t]->get_move() ) );
			}

			if( it == old_sorted.end() && all_idle ) {
				done = true;
			}
		}
		if( abort ) {
			std::cerr << "Aborted with " << evaluated << " moves evaluated at current depth" << std::endl;
		}

		res = alpha;
		if( alpha < result::loss_threshold || alpha > result::win_threshold ) {
			if( max_depth < MAX_DEPTH ) {
				std::cerr << "Early break at " << max_depth << std::endl;
			}
			abort = true;
		}

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


