#include "pawn_structure_hash_table.hpp"
#include "assert.hpp"

#include <string.h>
#include <stdlib.h>

struct pawn_structure_hash_table::entry
{
	uint64_t key;
	uint64_t data1;
	uint64_t data2;
	uint64_t data3;
};


pawn_structure_hash_table::pawn_structure_hash_table()
	: data_()
	, key_mask_()
	, init_size_()
{
}


pawn_structure_hash_table::~pawn_structure_hash_table()
{
	aligned_free( data_ );
}


bool pawn_structure_hash_table::init( uint64_t size_in_mib, bool reset )
{
	if( init_size_ != size_in_mib || reset ) {
		aligned_free( data_ );
		data_ = 0;
		init_size_ = size_in_mib;
#if USE_STATISTICS >= 2
		stats_.fill = 0;
#endif
	}

	while( !data_ && size_in_mib > 0 ) {
		uint64_t size = 4;
		while( size * 2 <= size_in_mib ) {
			size *= 2;
		}
		size *= 1024 * 1024;

		data_ = reinterpret_cast<entry*>(page_aligned_malloc( size ) );
		if( data_ ) {
			memset(data_, 0, size);
			key_mask_ = ((size/sizeof(entry)) - 1) << 5;
		}
		else {
			size_in_mib /= 2;
		}
	}

	return data_ != 0;
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


pawn_structure_hash_table::entry* pawn_structure_hash_table::get_entry( uint64_t key )
{
	uint64_t offset = key & key_mask_;
	return reinterpret_cast<entry*>(reinterpret_cast<unsigned char*>(data_) + offset);
}


pawn_structure_hash_table::entry const* pawn_structure_hash_table::get_entry( uint64_t key ) const
{
	uint64_t offset = key & key_mask_;
	return reinterpret_cast<entry const*>(reinterpret_cast<unsigned char const*>(data_) + offset);
}


void pawn_structure_hash_table::clear( uint64_t key )
{
	get_entry(key)->key = 0;
}


bool pawn_structure_hash_table::lookup( uint64_t key, score* eval, uint64_t& passed ) const
{
	entry const* entry = get_entry(key);
	
	uv1 v1;
	v1.p = entry->data1;
	uint64_t v2 = entry->data2;

	uint64_t dk = entry->key;

	if( (v1.p ^ v2 ^ dk) != key) {
#if USE_STATISTICS >= 2
		++stats_.misses;
#endif
		return false;
	}

	eval[0].mg() = v1.s.mg0;
	eval[1].mg() = v1.s.mg1;
	eval[0].eg() = v1.s.eg0;
	eval[1].eg() = v1.s.eg1;

	passed = v2;

#if USE_STATISTICS >= 2
	++stats_.hits;
#endif

	return true;
}


void pawn_structure_hash_table::store( uint64_t key, score const* eval, uint64_t passed )
{
	entry* entry = get_entry(key);

	uv1 v1;
	v1.s.mg0 = eval[0].mg();
	v1.s.mg1 = eval[1].mg();
	v1.s.eg0 = eval[0].eg();
	v1.s.eg1 = eval[1].eg();

	uint64_t v2 = passed;

#if USE_STATISTICS >= 2
	if( !entry->key ) {
		++stats_.fill;
	}
	else {
		++stats_.collision;
	}
#endif
	entry->data1 = v1.p;
	entry->data2 = v2;
	entry->key = v1.p ^ v2 ^ key;

#if VERIFY_PAWN_HASH_TABLE
	score ev2[2];
	uint64_t passed2;
	if( !lookup( key, ev2, passed2 ) ) {
		abort();
	}
	if( eval[0] != ev2[0] || eval[1] != ev2[1] || passed != passed2 ) {
		abort();
	}
#endif
}

#if USE_STATISTICS >= 2
pawn_structure_hash_table::stats pawn_structure_hash_table::get_stats( bool reset )
{
	stats ret = stats_;
	if( reset ) {
		stats_ = stats();
		stats_.fill = ret.fill;
	}

	return ret;
}

uint64_t pawn_structure_hash_table::max_hash_entry_count() const
{
	return (key_mask_ >> 5) + 1;
}
#endif