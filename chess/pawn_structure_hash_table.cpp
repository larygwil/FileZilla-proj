#include "pawn_structure_hash_table.hpp"
#include "statistics.hpp"

#include <string.h>

pawn_structure_hash_table pawn_hash_table;

struct pawn_structure_hash_table::entry
{
	unsigned long long key;
	unsigned long long data;
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


bool pawn_structure_hash_table::lookup( uint64_t key, short* eval ) const
{
	unsigned long long index = key % size_;

	unsigned long long v = data_[index].data;

	if( (v ^ data_[index].key) != key ) {
#if USE_STATISTICS
		stats_.misses++;
#endif
		return false;
	}

	eval[0] = static_cast<short>(v >> 16);
	eval[1] = static_cast<short>(v & 0xFFFFull);

#if USE_STATISTICS
	stats_.hits++;
#endif

	return true;
}


void pawn_structure_hash_table::store( uint64_t key, short const* eval )
{
	unsigned long long index = key % size_;

	unsigned long long v = (static_cast<uint64_t>(eval[0]) << 16) + static_cast<uint64_t>(eval[1]);

	data_[index].data = v;
	data_[index].key = v ^ key;
}


pawn_structure_hash_table::stats pawn_structure_hash_table::get_stats( bool reset )
{
	stats ret = stats_;
	if( reset ) {
		stats_ = stats();
	}

	return ret;
}
