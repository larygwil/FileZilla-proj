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
{
}


hash::~hash()
{
	delete [] data_;
}


bool hash::init( unsigned int max_size )
{
	size_ = static_cast<hash_key>(max_size) * 1024 * 1024;

	// Make sure size is a multiple of block size
	bucket_count_ = size_ / bucket_size;
	size_ = bucket_count_ * bucket_size;

	delete [] data_;
	data_ = 0;
	data_ = new entry[ bucket_count_ * bucket_entries ];
	memset( data_, 0, size_ );

	return true;
}


namespace field_shifts {
enum type {
	age = 0,
	depth = 8,
	move = 16,
	node_type = 46,
	score = 48
};
}

namespace field_masks {
enum type {
	age = 0xff,
	depth = 0xff,
	move = 0x1ffffff,
	node_type = 0x3,
	score = 0xffff
};
}

void hash::store( hash_key key, color::type c, unsigned char remaining_depth, short eval, short alpha, short beta, move const& best_move, unsigned char clock )
{
	if( c ) {
		key = ~key;
	}

	unsigned long long bucket_offset = (key % bucket_count_) * bucket_entries;
	entry* bucket = data_ + bucket_offset;

	uint64_t v = static_cast<unsigned long long>(clock) << field_shifts::age;
	v |= static_cast<unsigned long long>(remaining_depth) << field_shifts::depth;
	v |= (
				(static_cast<unsigned long long>(best_move.flags) ) |
				(static_cast<unsigned long long>(best_move.piece) << 5 ) |
				(static_cast<unsigned long long>(best_move.source_col) << 8) |
				(static_cast<unsigned long long>(best_move.source_row) << 11) |
				(static_cast<unsigned long long>(best_move.target_col) << 14) |
				(static_cast<unsigned long long>(best_move.target_row) << 17) |
				(static_cast<unsigned long long>(best_move.captured_piece) << 20) |
				(static_cast<unsigned long long>(best_move.promotion) << 23) ) << field_shifts::move;

	if( eval >= beta ) {
		v |= static_cast<unsigned long long>(score_type::lower_bound) << field_shifts::node_type;
	}
	else if( eval <= alpha ) {
		v |= static_cast<unsigned long long>(score_type::upper_bound) << field_shifts::node_type;
	}
	else {
		v |= static_cast<unsigned long long>(score_type::exact) << field_shifts::node_type;
	}
	v |= static_cast<unsigned long long>(eval) << field_shifts::score;

	for( unsigned int i = 0; i < bucket_entries; ++i ) {
		if( ((bucket + i)->v ^ (bucket + i)->key) == key ) {
			(bucket + i)->v = v;
			(bucket + i)->key = key ^ v;
			return;
		}
	}

	unsigned char lowest_depth = 255;
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
#endif
		pos->key = v ^ key;

		return;
	}

	lowest_depth = 255;
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
#endif
	pos->key = v ^ key;
}


score_type::type hash::lookup( hash_key key, color::type c, unsigned char remaining_depth, short alpha, short beta, short& eval, move& best_move, unsigned char clock )
{
	if( c ) {
		key = ~key;
	}

	unsigned long long bucket_offset = (key % bucket_count_) * bucket_entries;
	entry const* bucket = data_ + bucket_offset;

	for( unsigned int i = 0; i < bucket_entries; ++i, ++bucket ) {
		uint64_t v = bucket->v;
		if( (v ^ bucket->key) != key ) {
			continue;
		}

		best_move.flags = (v >> (field_shifts::move)) & 0x1F;
		best_move.piece = static_cast<pieces::type>((v >> (field_shifts::move + 5)) & 0x07);
		best_move.source_col = (v >> (field_shifts::move + 8)) & 0x07;
		best_move.source_row = (v >> (field_shifts::move + 11)) & 0x07;
		best_move.target_col = (v >> (field_shifts::move + 14)) & 0x07;
		best_move.target_row = (v >> (field_shifts::move + 17)) & 0x07;
		best_move.captured_piece = static_cast<pieces::type>((v >> (field_shifts::move + 20)) & 0x07);
		best_move.promotion = (v >> (field_shifts::move + 23)) & 0x03;

		unsigned char depth = (v >> field_shifts::depth) & field_masks::depth;

		if( depth >= remaining_depth ) {
			unsigned char type = (v >> field_shifts::node_type) & field_masks::node_type;
			eval = (v >> field_shifts::score) & field_masks::score;
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
		if( best_move.flags & move_flags::valid ) {
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

	best_move.flags = 0;
	return score_type::none;
}


hash::stats hash::get_stats(bool reset)
{
	stats ret = stats_;

	if( reset ) {
		stats_.hits = 0;
		stats_.misses = 0;
		stats_.best_move = 0;
	}
	return ret;
}

uint64_t hash::max_hash_entry_count() const
{
	return bucket_count_ * bucket_entries;
}
