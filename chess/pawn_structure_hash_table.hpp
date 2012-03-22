#ifndef __PAWN_STRUCTURE_HASH_TABLE_H__
#define __PAWN_STRUCTURE_HASH_TABLE_H__

#include "platform.hpp"
#include "score.hpp"

/*
 * Hash table to hold the pawn structure evaluation.
 * The general idea is the following:
 * Naive hash structure using always-replace.
 * Entry size is a power of two, thus somewhat cache-friendly.
 * Using Hyatt's lockless transposition table algorithm
 *
 * Key 64bit zobrist over pawns, white's point of view.
 *
 * Data is 2 times 16bit evaluation, for two game phases, 32 bit spare.
 *
 * If there are type-1 collisions, they are not handled the slightest.
 * Rationale being pawn evaluation only having a slight impact on overall
 * strength. Effort in handling collisions would exceed potential gain in
 * strengh. Also, pawn table collisions are more rare than transposition table
 * type-1 collisions due to lower number of pawn structures compared to full
 * board positions.
 */

class pawn_structure_hash_table
{
	struct entry;
public:
	class stats
	{
	public:
		stats()
			: hits()
			, misses()
		{
		}

		uint64_t hits;
		uint64_t misses;
	};

	pawn_structure_hash_table();
	~pawn_structure_hash_table();

	bool init( uint64_t size_in_mib );

	// Pass array of two shorts
	bool lookup( uint64_t key, score* eval, uint64_t& passed ) const;

	// Pass array of two shorts
	void store( uint64_t key, score const* eval, uint64_t passed );

	stats get_stats( bool reset );

	void clear( uint64_t key );

private:
	mutable stats stats_;

	entry* data_;
	uint64_t size_;
};


extern pawn_structure_hash_table pawn_hash_table;

#endif
