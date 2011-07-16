#include "hash.hpp"
#include "statistics.hpp"

#include <string.h>

namespace {
hash_key size_ = 0;
hash_key itemSize_ = 0;
hash_key blockSize_ = 0; // >= itemSize, may be larger than itemSize_ to pad
hash_key blockCount_;
unsigned char* data_ = 0;
}

void init_hash( unsigned int max_size, unsigned int itemSize )
{
	size_ = static_cast<hash_key>(max_size) * 1024 * 1024;
	itemSize_ = itemSize;
	blockSize_ = itemSize + sizeof(hash_key);

	// Make sure size is a multiple of block size
	blockCount_ = size_ / blockSize_;
	size_ = blockCount_ * blockSize_;

	delete [] data_;
	data_ = new unsigned char[size_];
	memset( data_, 0, size_ );
}


void free_hash() {
	delete [] data_;
	data_ = 0;
}

void clear_data()
{
	// todo: locking
	memset( data_, 0, size_ );
}

void store( hash_key key, unsigned char const* const data )
{
	// todo: locking
	unsigned long long const offset = (key % blockCount_) * blockSize_;

#if USE_STATISTICS
	hash_key old = *reinterpret_cast<hash_key*>( data_ + offset );
	if( old ) {
		if( old != key ) {
			++stats.transposition_table_collisions;
		}
	}
	else {
		++stats.transposition_table_num_entries;
	}
#endif

	*reinterpret_cast<hash_key*>( data_ + offset ) = key;
	memcpy( data_ + offset + sizeof( hash_key ), data, itemSize_ );
}

bool lookup( hash_key key, unsigned char *const data )
{
	unsigned long long const offset = (key % blockCount_) * blockSize_;

	hash_key const old = *reinterpret_cast<hash_key*>( data_ + offset );

	if( old != key ) {
#if USE_STATISTICS
		++stats.transposition_table_misses;
#endif
		return false;
	}
#if USE_STATISTICS
	else {
		++stats.transposition_table_hits;
	}
#endif

	memcpy( data, data_ + offset + sizeof( hash_key ), itemSize_ );

	return true;
}

unsigned long long max_hash_entry_count()
{
	return blockCount_;
}
