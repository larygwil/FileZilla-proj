#include "hash.hpp"

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
	memcpy( data_ + offset, &key, sizeof( hash_key) );

	memcpy( data_ + offset + sizeof( hash_key ), data, itemSize_ );
}

bool lookup( hash_key key, unsigned char *const data )
{
	unsigned long long const offset = (key % blockCount_) * blockSize_;
	if( memcmp( &key, data_ + offset, sizeof( hash_key ) ) ) {
		return false;
	}

	memcpy( data, data_ + offset + sizeof( hash_key ), itemSize_ );

	return true;
}
