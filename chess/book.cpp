#include "book.hpp"
#include "platform.hpp"

#include <fcntl.h>
#include <unistd.h>

int fd = -1;
int fd_index = -1;

mutex mtx;

namespace {
static void close_book_impl()
{
	if( fd != -1 ) {
		close(fd);
		fd = -1;
	}

	if( fd_index != -1 ) {
		close(fd_index);
		fd_index = -1;
	}
}
}


void close_book()
{
	scoped_lock l(mtx);
	close_book_impl();
}


bool open_book( std::string const& book_dir )
{
	scoped_lock l(mtx);
	close_book_impl();
	fd = open( (book_dir + "opening_book/book").c_str(), O_RDWR );
	fd_index = open( (book_dir + "opening_book/book_index").c_str(), O_RDWR );

	if( fd == -1 || fd_index == -1 ) {
		close_book_impl();
		return false;
	}

	return true;
}

namespace {
static unsigned long long get_offset_impl( unsigned long long index )
{
	lseek( fd_index, index * sizeof(unsigned long long), SEEK_SET );
	unsigned long long offset = 0;
	read( fd_index, &offset, sizeof(unsigned long long) );

	return offset;
}
}

unsigned long long get_offset( unsigned long long index )
{
	scoped_lock l(mtx);
	return get_offset_impl( index );
}

book_entry get_entries( unsigned long long index, std::vector<move_entry>& moves )
{
	scoped_lock l(mtx);

	unsigned long long offset = get_offset_impl(index);

	lseek( fd, offset, SEEK_SET );

	book_entry entry;
	read( fd, &entry, sizeof(book_entry) );

	move_entry move;
	while( true ) {
		read( fd, &move, sizeof(move_entry) );
		if( move.m.other ) {
			break;
		}

		moves.push_back( move );
	}

	return entry;
}

unsigned long long book_add_entry( book_entry const& b, std::vector<std::pair<short, move> > const& moves )
{
	scoped_lock l(mtx);

	unsigned long long index = lseek( fd_index, 0, SEEK_END ) / sizeof(unsigned long long);
	unsigned long long offset = lseek( fd, 0, SEEK_END );

	write( fd_index, &offset, sizeof( unsigned long long) );

	write( fd, &b, sizeof( book_entry ) );

	for( std::vector<std::pair<short, move> >::const_iterator it = moves.begin(); it != moves.end(); ++it ) {
		move_entry m;
		m.m = it->second;
		m.m.other = 0;
		m.forecast = it->first;
		m.next_index = 0;

		write( fd, &m, sizeof(move_entry) );
	}

	move_entry m;
	memset( &m, 0, sizeof(move_entry) );
	m.m.other = 1;
	write( fd, &m, sizeof(move_entry) );

	return index;
}

bool needs_init()
{
	scoped_lock l(mtx);

	unsigned long long index = lseek( fd_index, 0, SEEK_END ) / sizeof(unsigned long long);
	return index == 0;
}

void book_update_move( unsigned long long index, int move_index, unsigned long long new_index )
{
	scoped_lock l(mtx);

	unsigned long long offset = get_offset_impl( index );
	offset += sizeof(book_entry);
	offset += sizeof(move_entry) * move_index;
	offset += sizeof(move);
	offset += sizeof(short);

	lseek( fd, offset, SEEK_SET );

	write( fd, &new_index, sizeof(unsigned long long) );
}
