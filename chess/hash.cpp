#include "hash.hpp"
#include "statistics.hpp"
#include "zobrist.hpp"

#include <string.h>
#include <iostream>

hash transposition_table;

struct entry {
	uint64_t v;
	uint64_t key;
} PACKED;

uint64_t const bucket_entries = 4;
unsigned int const bucket_size = sizeof(entry) * bucket_entries;

hash::hash()
	: size_()
	, bucket_count_()
	, data_()
	, init_size_()
{
}


hash::~hash()
{
	aligned_free( data_ );
}


bool hash::init( unsigned int max_size )
{
	init_size_ = max_size;

	hash_key max = static_cast<hash_key>(max_size) * 1024 * 1024;

	size_ = 4 * 1024 * 1024;
	while( size_ * 2 < max ) {
		size_ *= 2;
	}

	// Make sure size is a multiple of block size
	bucket_count_ = size_ / bucket_size;
	aligned_free( data_ );
	data_ = reinterpret_cast<entry*>(page_aligned_malloc( bucket_count_ * bucket_size ));
	return data_ != 0;
}


bool hash::init_if_needed( unsigned int max_size )
{
	bool ret = true;

	if( !data_ || init_size_ != max_size ) {
		ret = init( max_size );
	}

	return ret;
}


void hash::clear_data()
{
	memset( data_, 0, size_ );
}


namespace field_shifts {
enum type {
	age = 0,
	depth = 8,
	move = 17,
	node_type = 46,
	score = 48
};
}

namespace field_masks {
enum type {
	age = 0xff,
	depth = 0x1ff,
	move = 0x1ffffff,
	node_type = 0x3,
	score = 0xffff
};
}

void hash::store( hash_key key, color::type c, unsigned short remaining_depth, unsigned char ply, short eval, short alpha, short beta, move const& best_move, unsigned char clock, short full_eval )
{
	if( c ) {
		key = ~key;
	}

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

	uint64_t bucket_offset = (key % bucket_count_) * bucket_entries;
	entry* bucket = data_ + bucket_offset;

	uint64_t v = static_cast<uint64_t>(clock) << field_shifts::age;
	v |= static_cast<uint64_t>(remaining_depth) << field_shifts::depth;
	v |= (
				(static_cast<uint64_t>(best_move.flags) ) |
				(static_cast<uint64_t>(best_move.piece) << 6 ) |
				(static_cast<uint64_t>(best_move.source) << 9) |
				(static_cast<uint64_t>(best_move.target) << 15) |
				(static_cast<uint64_t>(best_move.captured_piece) << 21) ) << field_shifts::move;

	v |= static_cast<uint64_t>(t) << field_shifts::node_type;
	v |= static_cast<uint64_t>(eval) << field_shifts::score;

	unsigned long long save_key = ((key ^ v) & 0xffffffffffff0000ull) | static_cast<unsigned short>(full_eval);

	for( unsigned int i = 0; i < bucket_entries; ++i ) {
		if( !((((bucket + i)->v ^ (bucket + i)->key) ^ key) & 0xffffffffffff0000ull ) ) {
			(bucket + i)->v = v;
			(bucket + i)->key = save_key;
			return;
		}
	}

	unsigned short lowest_depth = 511;
	entry* pos = 0;
	for( unsigned int i = 0; i < bucket_entries; ++i ) {
		unsigned char old_age = ((bucket + i)->v >> field_shifts::age) & field_masks::age;
		unsigned char old_depth = ((bucket + i)->v >> field_shifts::depth) & field_masks::depth;
		if( old_age != clock && old_depth < lowest_depth ) {
			lowest_depth = old_depth;
			pos = bucket + i;
		}
	}

	if( pos ) {
		pos->v = v;
#if USE_STATISTICS
		if( !pos->key ) {
			++stats_.entries;
		}
		else {
			++stats_.index_collisions;
		}
#endif
		pos->key = save_key;

		return;
	}

	lowest_depth = 511;
	for( unsigned int i = 0; i < bucket_entries; ++i ) {
		unsigned char old_depth = ((bucket + i)->v >> field_shifts::depth) & field_masks::depth;
		if( old_depth < lowest_depth ) {
			lowest_depth = old_depth;
			pos = bucket + i;
		}
	}

	pos->v = v;
#if USE_STATISTICS
	if( !pos->key ) {
		++stats_.entries;
	}
	else {
		++stats_.index_collisions;
	}
#endif
	pos->key = save_key;
}


score_type::type hash::lookup( hash_key key, color::type c, unsigned short remaining_depth, unsigned char ply, short alpha, short beta, short& eval, move& best_move, short& full_eval )
{
	if( c ) {
		key = ~key;
	}

	uint64_t bucket_offset = (key % bucket_count_) * bucket_entries;
	entry const* bucket = data_ + bucket_offset;

	for( unsigned int i = 0; i < bucket_entries; ++i, ++bucket ) {
		uint64_t v = bucket->v;
		uint64_t stored_key = bucket->key;
		if( ((v ^ stored_key) ^ key) & 0xffffffffffff0000ull ) {
			continue;
		}
		full_eval = static_cast<short>(static_cast<unsigned short>(stored_key & 0xFFFFull));

		best_move.flags = (v >> (field_shifts::move)) & 0x3F;
		best_move.piece = static_cast<pieces::type>((v >> (field_shifts::move + 6)) & 0x07);
		best_move.source = (v >> (field_shifts::move + 9)) & 0x3f;
		best_move.target = (v >> (field_shifts::move + 15)) & 0x3f;
		best_move.captured_piece = static_cast<pieces::type>((v >> (field_shifts::move + 21)) & 0x07);

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

			//unsigned char age = (v >> field_shifts::age) & field_masks::age;

			if( ( type == score_type::exact ) ||
				( type == score_type::lower_bound && beta <= eval ) ||
				( type == score_type::upper_bound && alpha >= eval ) )
			{
	#if USE_STATISTICS
				++stats_.hits;
	#endif
				return static_cast<score_type::type>(type);
			}
		}

#if USE_STATISTICS
		if( !best_move.empty() ) {
			++stats_.best_move;
		}
		else {
			++stats_.misses;
		}
#endif

		return score_type::none;
	}

#if USE_STATISTICS
	++stats_.misses;
#endif

	best_move.piece = pieces::none;
	return score_type::none;
}


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

uint64_t hash::max_hash_entry_count() const
{
	return bucket_count_ * bucket_entries;
}
