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
	delete [] data_;

	uint64_t size = size_in_mib * 1024 * 1024 / sizeof(entry);
	size_ = size;
	data_ = new entry[size_];

	memset(data_, 0, sizeof(entry) * size_);

	return true;
}


union uv1 {
	uint64_t p;
	struct {
		short mg0;
		short mg1;
		short eg0;
		short eg1;
	} s;
};


void pawn_structure_hash_table::clear( uint64_t key )
{
	uint64_t index = key % size_;
	data_[index].key = 0;
}


bool pawn_structure_hash_table::lookup( uint64_t key, score* eval, uint64_t& passed ) const
{
	uint64_t index = key % size_;

	uv1 v1;
	v1.p = data_[index].data1;
	uint64_t v2 = data_[index].data2;

	uint64_t dk = data_[index].key;

	if( (v1.p ^ v2 ^ dk) != key) {
#if USE_STATISTICS
		++stats_.misses;
#endif
		return false;
	}

	eval[0].mg() = v1.s.mg0;
	eval[1].mg() = v1.s.mg1;
	eval[0].eg() = v1.s.eg0;
	eval[1].eg() = v1.s.eg1;

	passed = v2;

#if USE_STATISTICS
	++stats_.hits;
#endif

	return true;
}


void pawn_structure_hash_table::store( uint64_t key, score const* eval, uint64_t passed )
{
	uint64_t index = key % size_;

	uv1 v1;
	v1.s.mg0 = eval[0].mg();
	v1.s.mg1 = eval[1].mg();
	v1.s.eg0 = eval[0].eg();
	v1.s.eg1 = eval[1].eg();

	uint64_t v2 = passed;

	data_[index].data1 = v1.p;
	data_[index].data2 = v2;
	data_[index].key = v1.p ^ v2 ^ key;

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
