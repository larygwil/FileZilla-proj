#include "hash.hpp"

#include <string.h>
#include <iostream>

struct entry {
	uint64_t v;
	uint64_t key;
} PACKED;

uint64_t const bucket_entries = 4;
unsigned int const bucket_size = sizeof(entry) * bucket_entries;

hash::hash()
	: size_()
	, key_mask_()
	, data_()
	, init_size_()
{
}


hash::~hash()
{
	aligned_free( data_ );
}


bool hash::init( unsigned int max_size, bool reset )
{
	if( init_size_ != max_size || reset ) {
		aligned_free( data_ );
		data_ = 0;
		init_size_ = max_size;
	}

	while( !data_ && max_size > 0 ) {
		hash_key max = static_cast<hash_key>(max_size) * 1024 * 1024;

		size_ = 4 * 1024 * 1024;
		while( size_ * 2 <= max ) {
			size_ *= 2;
		}

		// Make sure size is a multiple of block size
		ASSERT( !(size_ % bucket_size) );
		key_mask_ = (size_ / bucket_size) - 1;
		key_mask_ *= bucket_size;

		aligned_free( data_ );
		data_ = reinterpret_cast<entry*>(page_aligned_malloc( size_ ));

		if( !data_ ) {
			max_size /= 2;
		}
	}

	return data_ != 0;
}


void hash::clear_data()
{
	memset( data_, 0, size_ );
}


namespace field_shifts {
enum type : uint64_t {
	age = 0,
	depth = 8,
	move = 17,
	node_type = 33,
	score = 35
};
}

namespace field_masks {
enum type : uint64_t {
	age = 0xff,
	depth = 0x1ff,
	move = 0xffff,
	node_type = 0x3,
	score = 0xffff
};
}

void hash::store( hash_key key, unsigned short remaining_depth, unsigned char ply, short eval, short alpha, short beta, move const& best_move, unsigned char clock, short full_eval )
{
	score_type::type t;
	if( eval >= beta ) {
		t = score_type::lower_bound;
	}
	else if( eval <= alpha ) {
		t = score_type::upper_bound;
	}
	else {
		t = score_type::exact;
	}

	// Mate score adjustment.
	// Important: Do after determining the node type
	if( eval > result::win_threshold ) {
		eval += ply;
	}
	else if( eval < result::loss_threshold ) {
		eval -= ply;
	}

	uint64_t bucket_offset = key & key_mask_;
	entry* bucket = reinterpret_cast<entry*>(reinterpret_cast<unsigned char*>(data_) + bucket_offset);
	
	ASSERT( clock <= field_masks::age );
	ASSERT( remaining_depth <= field_masks::depth );
	ASSERT( t <= field_masks::node_type );
	ASSERT( static_cast<unsigned short>(eval) <= field_masks::score );
	
	uint64_t v = static_cast<uint64_t>(clock) << field_shifts::age;
	v |= static_cast<uint64_t>(remaining_depth) << field_shifts::depth;
	v |= static_cast<uint64_t>(t) << field_shifts::node_type;
	v |= static_cast<uint64_t>(static_cast<unsigned short>(eval)) << field_shifts::score;

	for( unsigned int i = 0; i < bucket_entries; ++i ) {
		uint64_t old_v = (bucket + i)->v;
		if( !(((old_v ^ (bucket + i)->key) ^ key) & 0xffffffffffc0003full ) ) {

			// If overwriting existing entry, copy existing move if we have none.
			// Otherwise we might end up with truncated pv.
			if( best_move.empty() ) {
				v |= old_v & (field_masks::move << field_shifts::move);
			}
			else {
				v |= static_cast<uint64_t>(best_move.d) << field_shifts::move;
			}

			unsigned long long save_key = ((key ^ v) & 0xffffffffffc0003full) | (static_cast<uint64_t>(static_cast<unsigned short>(full_eval)) << 6);
			(bucket + i)->v = v;
			(bucket + i)->key = save_key;
			return;
		}
	}

	v |= static_cast<uint64_t>(best_move.d) << field_shifts::move;
	unsigned long long save_key = ((key ^ v) & 0xffffffffffc0003full) | (static_cast<uint64_t>(static_cast<unsigned short>(full_eval)) << 6);

	unsigned short lowest_depth = 511;
	entry* pos = 0;
	for( unsigned int i = 0; i < bucket_entries; ++i ) {
		unsigned char old_age = ((bucket + i)->v >> field_shifts::age) & field_masks::age;
		unsigned short old_depth = ((bucket + i)->v >> field_shifts::depth) & field_masks::depth;
		if( old_age != clock && old_depth < lowest_depth ) {
			lowest_depth = old_depth;
			pos = bucket + i;
		}
	}

	if( !pos ) {
		lowest_depth = 511;
		for( unsigned int i = 0; i < bucket_entries; ++i ) {
			unsigned short old_depth = ((bucket + i)->v >> field_shifts::depth) & field_masks::depth;
			if( old_depth < lowest_depth ) {
				lowest_depth = old_depth;
				pos = bucket + i;
			}
		}
	}

	pos->v = v;
#if USE_STATISTICS >= 2
	if( !pos->key ) {
		add_relaxed( stats_.entries, 1 );
	}
	else {
		add_relaxed( stats_.index_collisions, 1 );
	}
#endif
	pos->key = save_key;
}


score_type::type hash::lookup( hash_key key, unsigned short remaining_depth, unsigned char ply, short alpha, short beta, short& eval, move& best_move, short& full_eval )
{
	uint64_t bucket_offset = key & key_mask_;
	entry const* bucket = reinterpret_cast<entry const*>(reinterpret_cast<unsigned char const*>(data_) + bucket_offset);

	for( unsigned int i = 0; i < bucket_entries; ++i, ++bucket ) {
		uint64_t v = bucket->v;
		uint64_t stored_key = bucket->key;
		if( ((v ^ stored_key) ^ key) & 0xffffffffffc0003full ) {
			continue;
		}
		full_eval = static_cast<short>(static_cast<unsigned short>((stored_key >> 6) & 0xFFFFull));

		best_move.d = (v >> field_shifts::move) & field_masks::move;

		unsigned short depth = (v >> field_shifts::depth) & field_masks::depth;

		if( depth >= remaining_depth ) {
			unsigned char type = (v >> field_shifts::node_type) & field_masks::node_type;
			eval = (v >> field_shifts::score) & field_masks::score;

			if( eval > result::win_threshold ) {
				eval -= ply;
			}
			else if( eval < result::loss_threshold ) {
				eval += ply;
			}

			if( ( type == score_type::exact ) ||
				( type == score_type::lower_bound && beta <= eval ) ||
				( type == score_type::upper_bound && alpha >= eval ) )
			{
#if USE_STATISTICS >= 2
				add_relaxed( stats_.hits, 1 );
#endif
				return static_cast<score_type::type>(type);
			}
		}

#if USE_STATISTICS >= 2
		if( !best_move.empty() ) {
			add_relaxed( stats_.best_move, 1 );
		}
		else {
			add_relaxed( stats_.misses, 1 );
		}
#endif

		return score_type::none;
	}

#if USE_STATISTICS >= 2
	add_relaxed( stats_.misses, 1 );
#endif

	return score_type::none;
}


uint64_t hash::max_hash_entry_count() const
{
	return size_ / bucket_size * bucket_entries;
}


unsigned short hash::max_depth()
{
	return field_masks::depth - 1;
}


#if USE_STATISTICS >= 2
hash::stats hash::get_stats(bool reset)
{
	stats ret = stats_;

	if( reset ) {
		stats_.hits = 0;
		stats_.misses = 0;
		stats_.index_collisions = 0;
		stats_.best_move = 0;
	}
	return ret;
}


hash::stats::stats()
	: entries()
	, hits()
	, best_move()
	, misses()
	, index_collisions()
{
	ASSERT( entries.is_lock_free() );
}


hash::stats::stats( stats const& s )
{
	atomic_store( entries, s.entries );
	atomic_store( hits, s.hits );
	atomic_store( best_move, s.best_move );
	atomic_store( misses, s.misses );
	atomic_store( index_collisions, s.index_collisions );
}


hash::stats& hash::stats::operator=( stats const& s )
{
	if( this != &s ) {
		atomic_store( entries, s.entries );
		atomic_store( hits, s.hits );
		atomic_store( best_move, s.best_move);
		atomic_store( misses, s.misses);
		atomic_store( index_collisions, s.index_collisions );
	}

	return *this;
}


#endif