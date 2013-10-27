#include "simple_book.hpp"
#include "moves.hpp"

#include <fstream>
#include <algorithm>

unsigned short const simple_book_version = 1;

class simple_book::impl
{
public:
	impl()
		: header_size()
		, moves_per_entry()
		, entry_size()
		, size()
	{
	}

	uint64_t header_size;
	uint64_t moves_per_entry;
	uint64_t entry_size;
	uint64_t size;
	std::ifstream in;
};


simple_book::simple_book( std::string const& book_dir )
	: impl_()
{
	open( book_dir );
}


simple_book::~simple_book()
{
	close();
}


bool simple_book::open( std::string const& book_dir )
{
	close();

	impl_ = new impl;
	impl_->in.open( book_dir + "octochess.book", std::ios::binary );

	if( !impl_->in.is_open() ) {
		close();
		return false;
	}

	impl_->in.seekg( 0, impl_->in.end );
	uint64_t length = impl_->in.tellg();
	impl_->in.seekg( 0, impl_->in.beg );

	if( length < 5 ) {
		close();
		return false;
	}

	unsigned char tmp[5];
	impl_->in.read( reinterpret_cast<char*>(tmp), 5 );

	impl_->header_size = tmp[0] + static_cast<uint64_t>(tmp[1]) * 256;
	if( impl_->header_size < 5 || length < impl_->header_size ) {
		close();
		return false;
	}

	unsigned short header_version = tmp[2] + static_cast<unsigned short>(tmp[3]) * 256;
	if( header_version != simple_book_version ) {
		close();
		return false;
	}

	impl_->moves_per_entry = tmp[4];
	impl_->entry_size = 8 + impl_->moves_per_entry * 3;
	if( !impl_->moves_per_entry || (length - impl_->header_size) % impl_->entry_size ) {
		close();
		return false;
	}

	impl_->size = (length - impl_->header_size) / impl_->entry_size;

	return true;
}


void simple_book::close()
{
	delete impl_;
	impl_ = 0;
}


bool simple_book::is_open() const
{
	return impl_ != 0;
}


std::vector<simple_book_entry> simple_book::get_entries( position const& p )
{
	std::vector<simple_book_entry> ret;

	if( !is_open() ) {
		return ret;
	}

	uint64_t min = 0;
	uint64_t max = impl_->size;

	while( min < max ) {
		uint64_t mid = min + (max - min) / 2;

		uint64_t offset = impl_->header_size + mid * impl_->entry_size;
		impl_->in.seekg( offset, impl_->in.beg );

		unsigned char tmp[8] = {0};
		impl_->in.read( reinterpret_cast<char*>(tmp), 8 );

		uint64_t hash = 0;
		for( uint64_t i = 0; i < 8; ++i ) {
			hash |= static_cast<uint64_t>(tmp[i]) << (i * 8);
		}
		if( hash < p.hash_ ) {
			min = mid + 1;
		}
		else if( hash > p.hash_ ) {
			max = mid;
		}
		else {
			auto moves = calculate_moves<movegen_type::all>( p );
			std::stable_sort( moves.begin(), moves.end() );

			for( uint64_t i = 0; i < impl_->moves_per_entry; ++i ) {
				impl_->in.read( reinterpret_cast<char*>(tmp), 3 );
				simple_book_entry e;

				unsigned char mi = tmp[0];
				if( !mi ) {
					continue;
				}
				--mi;

				if( mi >= moves.size() ) {
					// Huh, book broken?
					continue;
				}
				e.m = moves[mi];

				e.forecast = static_cast<short>(tmp[1] + static_cast<unsigned short>(tmp[2]) * 256);
				ret.push_back( e );
			}
			return ret;
		}
	}

	return ret;
}
