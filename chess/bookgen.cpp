#include "book.hpp"
#include "calc.hpp"
#include "eval.hpp"
#include "hash.hpp"
#include "moves.hpp"
#include "platform.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <signal.h>
#include <iostream>

int const MAX_BOOK_DEPTH = 10;

namespace {
typedef std::vector<std::pair<short, move> > sorted_moves;
void insert_sorted( sorted_moves& moves, short forecast, move const& m )
{
	sorted_moves::iterator it = moves.begin();
	while( it != moves.end() && it->first >= forecast ) {
		++it;
	}
	moves.insert( it, std::make_pair(forecast, m ) );
}
}

unsigned long long calculate_position( position const& p, color::type c, int depth )
{
	book_entry entry;
	entry.reached_at_depth = depth;
	entry.full_depth = MAX_DEPTH;
	entry.quiescence_depth = QUIESCENCE_SEARCH;

	unsigned long long hash = get_zobrist_hash( p, c );

	short eval = evaluate( p, c );

	move_info moves[200];
	move_info* pm = moves;
	check_map check;
	calc_check_map( p, c, check );
	calculate_moves( p, c, eval, pm, check );

	sorted_moves moves_with_forecast;

	for( move_info const* it = moves; it != pm; ++it ) {
		position new_pos = p;
		bool capture = apply_move( new_pos, it->m, c );

		unsigned long long new_hash = update_zobrist_hash( p, c, hash, it->m );
		short value = -step( 1, MAX_DEPTH, new_pos, new_hash, -it->evaluation, capture, static_cast<color::type>(1-c), result::loss, result::win );

		insert_sorted( moves_with_forecast, value, it->m );
	}

	return book_add_entry( entry, moves_with_forecast );
}


void init_book()
{
	position p;
	init_board(p);

	calculate_position( p, color::white, 0 );
}

struct work {
	int depth;
	position p;
	color::type c;
	unsigned long long index; // position
	int move_index; // move inside position
};

struct worklist {
	std::vector<work> work_at_depth[MAX_BOOK_DEPTH];
	int next_work;
};


void get_work( worklist& wl, int max_depth, int max_width, int depth, unsigned long long index, position const& p, color::type c )
{
	std::vector<move_entry> moves;
	/*book_entry entry = */
	get_entries( index, moves );

	unsigned char added_at_pos = 0;
	for( std::vector<move_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
		position new_pos = p;
		apply_move( new_pos, it->m, c );

		if( !it->next_index ) {

			if( added_at_pos < (moves.size() + 1) ) {
				++added_at_pos;
				work w;
				w.depth = depth;
				w.index = index;
				w.move_index = it - moves.begin();
				w.p = new_pos;
				w.c = static_cast<color::type>(1-c);
				wl.work_at_depth[depth].push_back(w);
			}
		}
		else {
			++added_at_pos;
			if( depth + 1 < max_depth ) {
				get_work( wl, max_depth, max_width, depth + 1, it->next_index, new_pos, static_cast<color::type>(1-c) );
			}
		}
	}
}


void get_work( worklist& wl, int max_depth, int max_width )
{
	wl.next_work = 0;

	position p;
	init_board( p );

	get_work( wl, max_depth, max_width, 1, 0, p, color::white );
}

bool get_next( worklist& wl, work& w )
{
	int i = wl.next_work;
	do {
		if( !wl.work_at_depth[i].empty() ) {
			w = wl.work_at_depth[i].front();
			wl.work_at_depth[i].erase( wl.work_at_depth[i].begin() );

			if( ++wl.next_work == MAX_BOOK_DEPTH ) {
				wl.next_work = 0;
			}

			return true;
		}
		if( ++i == MAX_BOOK_DEPTH ) {
			i = 0;
		}
	} while( i != wl.next_work );

	if( !wl.work_at_depth[i].empty() ) {
		w = wl.work_at_depth[i].front();
		wl.work_at_depth[i].erase( wl.work_at_depth[i].begin() );

		if( ++wl.next_work == MAX_BOOK_DEPTH ) {
			wl.next_work = 0;
		}

		return true;
	}

	return false;
}

volatile bool stop = false;


extern "C" void on_signal(int)
{
	stop = true;
}

int main( int argc, char const* argv[] )
{
	std::string book_dir;
	std::string self = argv[0];
	if( self.rfind('/') != std::string::npos ) {
		book_dir = self.substr( 0, self.rfind('/') + 1 );
	}

	signal( SIGINT, &on_signal );

	console_init();

	init_random( 1234 );
	init_zobrist_tables();

	init_hash( 2048+1024, sizeof(step_data) );

	open_book( book_dir );

	if( needs_init() ) {
		init_book();
	}

	int max_depth = 4;
	int max_width = 2;

	while( !stop ) {

		worklist wl;
		get_work( wl, max_depth, max_width );

		work w;
		while( !stop && get_next( wl, w ) ) {
			unsigned long long index = calculate_position( w.p, w.c, w.depth );
			book_update_move( w.index, w.move_index, index );
		}

		if( max_depth < MAX_BOOK_DEPTH  ) {
			++max_depth;
		}
		else {
			std::cerr << "All done" << std::endl;
			break;
		}
	}

	return 0;
}
