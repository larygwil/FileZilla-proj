#include "book.hpp"
#include "platform.hpp"
#include "util.hpp"

#include <iostream>
#include <list>
#include <sstream>

#include <fcntl.h>
#include <unistd.h>

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

bool open_book( std::string const& book_dir )
{
	scoped_lock l(impl.mtx_);
	close_book_impl();
	impl.fd_ = open( (book_dir + "opening_book/book").c_str(), O_RDWR );
	impl.fd_index_ = open( (book_dir + "opening_book/book_index").c_str(), O_RDWR );

	if( impl.fd_ == -1 || impl.fd_index_ == -1 ) {
		close_book_impl();
		return false;
	}

	return true;
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
	scoped_lock l(impl.mtx_);

	unsigned long long index = lseek( impl.fd_index_, 0, SEEK_END ) / sizeof(unsigned long long);
	unsigned long long offset = lseek( impl.fd_, 0, SEEK_END );

	b.count_moves = moves.size();

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

		apply_move( p, *it, c );
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
