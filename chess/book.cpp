#include "book.hpp"

#include <fcntl.h>
#include <unistd.h>

int fd = -1;
int fd_index = -1;

void close_book()
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


bool open_book( std::string const& book_dir )
{
	close_book();
	fd = open( (book_dir + "opening_book/book").c_str(), O_RDWR );
	fd_index = open( (book_dir + "opening_book/book_index").c_str(), O_RDWR );

	if( fd == -1 || fd_index == -1 ) {
		close_book();
		return false;
	}

	return true;
}

unsigned long long get_offset( unsigned long long index )
{
	lseek( fd_index, index * sizeof(unsigned long long), SEEK_SET );
	unsigned long long offset = 0;
	read( fd_index, &offset, sizeof(unsigned long long) );

	return offset;
}

book_entry get_entries( unsigned long long index, std::vector<move_entry>& moves )
{
	unsigned long long offset = get_offset(index);

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
	unsigned long long index = lseek( fd_index, 0, SEEK_END ) / sizeof(unsigned long long);
	return index == 0;
}

void book_update_move( unsigned long long index, int move_index, unsigned long long new_index )
{
	unsigned long long offset = get_offset( index );
	offset += sizeof(book_entry);
	offset += sizeof(move_entry) * move_index;
	offset += sizeof(move);
	offset += sizeof(short);

	lseek( fd, offset, SEEK_SET );

	write( fd, &new_index, sizeof(unsigned long long) );
}
