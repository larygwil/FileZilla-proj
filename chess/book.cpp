#include "assert.hpp"
#include "book.hpp"
#include "platform.hpp"
#include "util.hpp"

#include <deque>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <sstream>

#include <fcntl.h>
#ifdef __GNUG__
#include <unistd.h>
#else
#pragma warning(disable:4996)
#include <io.h>
#include <sys/stat.h>
#define lseek _lseeki64
#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#endif

void close_book();

class book_impl {
public:
	book_impl()
		: fd_(-1)
		, fd_index_(-1)
	{}

	~book_impl() {
		close_book();
	}

	int fd_;
	int fd_index_;

	mutex mtx_;

	std::string book_dir_;
};

book_impl impl;

namespace {
static void close_book_impl()
{
	if( impl.fd_ != -1 ) {
		close(impl.fd_);
		impl.fd_ = -1;
	}

	if( impl.fd_index_ != -1 ) {
		close(impl.fd_index_);
		impl.fd_index_ = -1;
	}
}
}


void close_book()
{
	scoped_lock l(impl.mtx_);
	close_book_impl();
}

namespace {
static bool open_book_impl( std::string const& book_dir )
{
	close_book_impl();
	impl.book_dir_ = book_dir;
	impl.fd_ = open( (book_dir + "opening_book/book").c_str(), O_RDWR );
	impl.fd_index_ = open( (book_dir + "opening_book/book_index").c_str(), O_RDWR );

	if( impl.fd_ == -1 || impl.fd_index_ == -1 ) {
		close_book_impl();
		return false;
	}

	return true;
}
}

bool open_book( std::string const& book_dir )
{
	scoped_lock l(impl.mtx_);
	return open_book_impl( book_dir );
}

namespace {
static unsigned long long get_offset_impl( unsigned long long index )
{
	lseek( impl.fd_index_, index * sizeof(unsigned long long), SEEK_SET );
	unsigned long long offset = 0;
	read( impl.fd_index_, &offset, sizeof(unsigned long long) );

	return offset;
}
}

unsigned long long get_offset( unsigned long long index )
{
	scoped_lock l(impl.mtx_);
	return get_offset_impl( index );
}

namespace {
static book_entry get_entry_impl( unsigned long long index )
{
	unsigned long long offset = get_offset_impl(index);

	lseek( impl.fd_, offset, SEEK_SET );

	book_entry entry;
	read( impl.fd_, &entry, sizeof(book_entry) );

	return entry;
}

static book_entry get_entries_impl( unsigned long long index, std::vector<move_entry>& moves )
{
	unsigned long long offset = get_offset_impl(index);

	lseek( impl.fd_, offset, SEEK_SET );

	book_entry entry;
	read( impl.fd_, &entry, sizeof(book_entry) );

	move_entry move;
	for( unsigned char i = 0; i < entry.count_moves; ++i ) {
		read( impl.fd_, &move, sizeof(move_entry) );
		moves.push_back( move );
	}

	return entry;
}
}

book_entry get_entry( unsigned long long index )
{
	scoped_lock l(impl.mtx_);
	return get_entry_impl( index );
}

book_entry get_entries( unsigned long long index, std::vector<move_entry>& moves )
{
	moves.clear();

	scoped_lock l(impl.mtx_);
	return get_entries_impl( index, moves );
}

unsigned long long book_add_entry( book_entry b, std::vector<move_entry> const& moves )
{
	ASSERT(moves.size() <= 255);
	scoped_lock l(impl.mtx_);

	unsigned long long index = lseek( impl.fd_index_, 0, SEEK_END ) / sizeof(unsigned long long);
	unsigned long long offset = lseek( impl.fd_, 0, SEEK_END );

	b.count_moves = static_cast<unsigned char>(moves.size());

	write( impl.fd_index_, &offset, sizeof( unsigned long long) );

	write( impl.fd_, &b, sizeof( book_entry ) );


	for( std::vector<move_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
		move_entry m = *it;
		m.next_index = 0;

		write( impl.fd_, &m, sizeof(move_entry) );
	}

	return index;
}

bool needs_init()
{
	scoped_lock l(impl.mtx_);

	unsigned long long index = lseek( impl.fd_index_, 0, SEEK_END ) / sizeof(unsigned long long);
	return index == 0;
}

void book_update_move( unsigned long long index, int move_index, unsigned long long new_index )
{
	scoped_lock l(impl.mtx_);

	unsigned long long offset = get_offset_impl( index );
	offset += sizeof(book_entry);
	offset += sizeof(move_entry) * move_index;

	lseek( impl.fd_, offset, SEEK_SET );

	move_entry e;
	read( impl.fd_, &e, sizeof(move_entry) );
	e.next_index = new_index;

	lseek( impl.fd_, offset, SEEK_SET );
	write( impl.fd_, &e, sizeof(move_entry) );
}

move move_entry::get_move() const
{
	move m;
	m.source_col = source_col;
	m.source_row = source_row;
	m.target_col = target_col;
	m.target_row = target_row;
	return m;
}

void move_entry::set_move( move const& m )
{
	source_col = m.source_col;
	source_row = m.source_row;
	target_col = m.target_col;
	target_row = m.target_row;
}

bool move_entry::operator>=( move_entry const& rhs ) const
{
	if( forecast > rhs.forecast ) {
		return true;
	}
	if( forecast < rhs.forecast ) {
		return false;
	}

	if( full_depth > rhs.full_depth ) {
		return true;
	}
	if( full_depth > rhs.full_depth ) {
		return false;
	}

	return quiescence_depth >= rhs.quiescence_depth;
}

std::string line_string( unsigned long long index )
{
	std::stringstream ss;
	std::list<move> moves;

	{
		scoped_lock l(impl.mtx_);

		while( index ) {
			book_entry bentry = get_entry_impl( index );

			std::vector<move_entry> parent_moves;
			get_entries_impl( bentry.parent_index, parent_moves );

			unsigned int i;
			for( i = 0; i < parent_moves.size(); ++i ) {
				if( parent_moves[i].next_index == index ) {
					index = bentry.parent_index;
					moves.push_front( parent_moves[i].get_move() );
					break;
				}
			}

			if( i == parent_moves.size() ) {
				std::cerr << "Corrupt book! Failed to track back line from index " << index << std::endl;
				exit(1);
			}
		}
	}

	position p;
	init_board( p );

	color::type c = color::white;

	int i = 0;
	for( std::list<move>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
		if( i ) {
			ss << " ";
		}
		if( c == color::white ) {
			ss << ++i << ". ";
		}
		else {
			ss << " ";
		}
		ss << move_to_string( p, c, *it );

		bool captured;
		apply_move( p, *it, c, captured );
		c = static_cast<color::type>(1-c);
	}

	return ss.str();
}

namespace {
unsigned long long fix_depth_impl( unsigned long long index, unsigned char reached_at_depth )
{
	unsigned long long ret = 0;

	std::vector<move_entry> moves;
	book_entry bentry = get_entries_impl( index, moves );
	if( bentry.reached_at_depth > reached_at_depth ) {
		++ret;
		bentry.reached_at_depth = reached_at_depth;
		unsigned long long offset = get_offset_impl( index );
		lseek( impl.fd_, offset, SEEK_SET );
		write( impl.fd_, &bentry, sizeof(book_entry) );
	}

	for( unsigned int i = 0; i < moves.size(); ++i ) {
		if( moves[i].next_index ) {
			ret += fix_depth_impl( moves[i].next_index, bentry.reached_at_depth + 1 );
		}
	}

	return ret;
}
}

unsigned long long fix_depth( unsigned long long index, unsigned char reached_at_depth )
{
	scoped_lock l(impl.mtx_);
	return fix_depth_impl( index, reached_at_depth );
}

namespace {
void set_parent_impl( unsigned long long index, unsigned long long parent )
{
	book_entry entry = get_entry_impl( index );
	entry.parent_index = parent;
	unsigned long long offset = get_offset_impl( index );
	lseek( impl.fd_, offset, SEEK_SET );
	write( impl.fd_, &entry, sizeof(book_entry) );
}
}

void set_parent( unsigned long long index, unsigned long long parent )
{
	scoped_lock l(impl.mtx_);
	set_parent_impl( index, parent );
}

void vacuum_book()
{
	scoped_lock l(impl.mtx_);

	unsigned long long old_count = lseek( impl.fd_index_, 0, SEEK_END ) / 8;

	// Breath-first
	struct TranspositionWork {
		position p;
		color::type c;
		unsigned long long index;
	};
	std::deque<TranspositionWork> toVisit;

	std::map<unsigned long long, unsigned long long> index_map;

	{
		// Get started
		{
			TranspositionWork w;
			init_board( w.p );
			w.c = color::white;
			w.index = 0;

			toVisit.push_back( w );
		}

		while( !toVisit.empty() ) {
			TranspositionWork const w = toVisit.front();
			toVisit.pop_front();

			index_map.insert( std::make_pair( w.index, index_map.size()) );

			std::vector<move_entry> moves;
			get_entries_impl( w.index, moves );

			for( std::vector<move_entry>::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
				if( !it->next_index ) {
					continue;
				}

				TranspositionWork new_work;
				new_work.c = static_cast<color::type>(1-w.c);
				new_work.p = w.p;
				new_work.index = it->next_index;
				bool captured;
				apply_move( new_work.p, it->get_move(), new_work.c, captured );
				toVisit.push_back( new_work );
			}
		}
	}

	if( index_map.size() == old_count ) {
		return;
	}

	std::cerr << "Vaccuuming " << old_count - index_map.size() << " positions" << std::endl;

	int fd_vacuum = open( (impl.book_dir_ + "opening_book/book_vacuum").c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR|S_IWUSR );
	int fd_index_vacuum = open( (impl.book_dir_ + "opening_book/book_index_vacuum").c_str(), O_CREAT | O_EXCL | O_RDWR, S_IRUSR|S_IWUSR );

	if( fd_vacuum == -1 || fd_index_vacuum == -1 ) {
		std::cerr << "Failed to open vacuum files!" << std::endl;
		return;
	}

	// Reverse mapping
	std::map<unsigned long long, unsigned long long> index_map_reversed;
	for( std::map<unsigned long long, unsigned long long>::const_iterator it = index_map.begin(); it != index_map.end(); ++it )	{
		index_map_reversed[it->second] = it->first;
	}
	if( index_map.size() != index_map_reversed.size() ) {
		std::cerr << "Book algorithms are faulty. Abandon ship!" << std::endl;
		exit(1);
	}

	// Now go over it again, update indexes and write to new file
	for( std::map<unsigned long long, unsigned long long>::const_iterator it = index_map_reversed.begin(); it != index_map_reversed.end(); ++it ) {

		std::vector<move_entry> moves;
		book_entry bentry = get_entries_impl( it->second, moves );

		unsigned long long offset = lseek( fd_vacuum, 0, SEEK_END );

		std::map<unsigned long long, unsigned long long>::const_iterator iit = index_map.find( bentry.parent_index );
		if( iit == index_map.end() ) {
			std::cerr << "Corrupt book! Missing parent entry" << std::endl;
			exit(1);
		}
		bentry.parent_index = iit->second;
		write( fd_vacuum, &bentry, sizeof(book_entry) );

		lseek( fd_index_vacuum, it->first * 8, SEEK_SET );
		write( fd_index_vacuum, &offset, 8 );

		for( std::vector<move_entry>::const_iterator mit = moves.begin(); mit != moves.end(); ++mit ) {
			move_entry entry = *mit;
			if( entry.next_index ) {
				std::map<unsigned long long, unsigned long long>::const_iterator iit = index_map.find( entry.next_index );
				if( iit == index_map.end() ) {
					std::cerr << "Corrupt book! Missing move entry" << std::endl;
					exit(1);
				}
				entry.next_index = iit->second;
			}
			write( fd_vacuum, &entry, sizeof(move_entry) );
		}
	}

	close( fd_vacuum );
	close( fd_index_vacuum );

	close_book_impl();
	rename( (impl.book_dir_ + "opening_book/book_vacuum").c_str(), (impl.book_dir_ + "opening_book/book").c_str() );
	rename( (impl.book_dir_ + "opening_book/book_index_vacuum").c_str(), (impl.book_dir_ + "opening_book/book_index").c_str() );

	open_book_impl( impl.book_dir_ );
}

book_stats get_book_stats()
{
	book_stats s;
	s.unique_positions = 0;
	//s.transpositions = 0;

	unsigned long long total_moves = 0;

	scoped_lock l(impl.mtx_);
	s.unique_positions = lseek( impl.fd_index_, 0, SEEK_END ) / 8;


	for( unsigned long long i = 0; i < s.unique_positions; ++i ) {
		book_entry bentry = get_entry_impl( i );
		total_moves += bentry.count_moves;

		s.positions_at_depth.insert( std::make_pair( bentry.reached_at_depth, 0 ) );
		++s.positions_at_depth[bentry.reached_at_depth];
	}

	s.average_moves_per_position = total_moves / double(s.unique_positions);

	return s;
}

void print_book_stats()
{
	book_stats s = get_book_stats();

	std::cout << "Number of unique positions in book: " << s.unique_positions << std::endl;
	std::cout << "Average number of moves per position: " << s.average_moves_per_position << std::endl;
	std::cout << "Depth statistics:" << std::endl;
	for( std::map<int, unsigned long long>::const_iterator it = s.positions_at_depth.begin(); it != s.positions_at_depth.end(); ++it ) {
		std::cout << "  " << std::setw( 3 ) << it->first;
		std::cout << "  " << std::setw( 5 ) << it->second << std::endl;
	}
}
