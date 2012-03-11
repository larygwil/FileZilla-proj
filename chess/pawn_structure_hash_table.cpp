#include "pawn_structure_hash_table.hpp"
#include "statistics.hpp"

#include <string.h>
#include <stdlib.h>

pawn_structure_hash_table pawn_hash_table;

struct pawn_structure_hash_table::entry
{
	uint64_t key;
	uint64_t data1;
	uint64_t data2;
	uint64_t data3;
};


pawn_structure_hash_table::pawn_structure_hash_table()
	: data_()
	, size_()
{
}


pawn_structure_hash_table::~pawn_structure_hash_table()
{
	delete [] data_;
}


bool pawn_structure_hash_table::init( uint64_t size_in_mib )
{
	uint64_t size = size_in_mib * 1024 * 1024 / sizeof(entry);
	if( !data_ || size != size_ ) {
		size_ = size;
		delete [] data_;
		data_ = new entry[size_];

		memset(data_, 0, sizeof(entry) * size_);
	}

	return true;
}


bool pawn_structure_hash_table::lookup( uint64_t key, short* eval, uint64_t& passed ) const
{
	uint64_t index = key % size_;

	uint64_t v1 = data_[index].data1;
	uint64_t v2 = data_[index].data2;

	uint64_t dk = data_[index].key;

	if( (v1 ^ v2 ^ dk) != key) {
#if USE_STATISTICS
		++stats_.misses;
#endif
		return false;
	}

	eval[0] = static_cast<short>(v1 >> 16);
	eval[1] = static_cast<short>(v1 & 0xFFFFull);

	passed = v2;

#if USE_STATISTICS
	++stats_.hits;
#endif

	return true;
}


void pawn_structure_hash_table::store( uint64_t key, short const* eval, uint64_t passed )
{
	uint64_t index = key % size_;

	uint64_t v1 = (static_cast<uint64_t>(static_cast<unsigned short>(eval[0])) << 16) + static_cast<uint64_t>(static_cast<unsigned short>(eval[1]));
	uint64_t v2 = passed;

	data_[index].data1 = v1;
	data_[index].data2 = v2;
	data_[index].key = v1 ^ v2 ^ key;

#if 0
	short ev2[2];
	if( !lookup( key, ev2 ) ) {
		abort();
	}
	if( eval[0] != ev2[0] || eval[1] != ev2[1] ) {
		abort();
	}
#endif
}


pawn_structure_hash_table::stats pawn_structure_hash_table::get_stats( bool reset )
{
	stats ret = stats_;
	if( reset ) {
		stats_ = stats();
	}

	return ret;
}
