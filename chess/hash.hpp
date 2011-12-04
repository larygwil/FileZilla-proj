#ifndef __HASH_H__
#define __HASH_H__

#include "chess.hpp"
#include "platform.hpp"

/*
 * General considerations:
 * 1) No expensive memory allocation stuff
 * 2) Fast
 * 3) High hit rate
 *
 * From 1) follows that we need a fixed-size memory block, allocated at
 * startup and just used from there on.
 *
 * From 2) follows that we need to keep things like the cache lines in mind.
 * While optimizing Octochess and analysing where performance is spent, I have
 * noticed that a suprisingly large amount of performance is spent at what can
 * only be attributed to lack of memory bandwidth. So we should try to exploit
 * things like cache lines.
 *
 * From 3) follows that, once the cache has filled to some degree, a suitable
 * replacement scheme is needed. Practical results in Octochess with using
 * naive hashing without buckets and using always-replace strategy have shown
 * that we get as much as 10k type-2 hash collisions (same memory location,
 * different key) with a 3GiB cache at 8+5 search depth even at the initial
 * position. Needless to say, at higher search-depths mid-game with a full
 * table, collisions become insanely high and hitrate suffers a lot due to
 * many valuable entries being replaced constantly.
 *
 * Available online literature suggests that using a bucket based approach
 * yields best performance, so I'll do just that. I made some preliminary
 * thoughts about using a multi-tiered approach, though that wouldn't solve
 * the replacement strategy problem but would further make it difficult to
 * find an optimal split in what sizes to choose for each tier, where it would
 * likely be best to adjust it dynamically which would directly contradict
 * requirement 1).
 *
 * Using an bucket approach, two things are still missing: Number of buckets
 * and a replacement strategy.
 * Number of buckets should play along nicely with point 2), exploiting the
 * cache line size.
 *
 * We have:
 * - 64 bit zobrist hash of position
 * - 16 bit score
 * -  2 bit node type
 * - 25 bit move (row/col for source and target: 4 * 3 bit, piece: 3 bit, flags: 5 bit, capture: 3 bit, promotion: 2 bit)
 * -  8 bit remaining depth
 * -  8 bit age
 * Total: 123 bits.
 *
 * To avoid expensive calculations, we need to at the very least byte-align
 * the entries, so we have 128 bits. My CPUs (all of which are Intel Core i7)
 * have cache lines of 64 bytes, so we could fit 4 entries in it.
 *
 * If we were to spare a few more bits, we could even squeeze another position
 * into the bucket. We could do so, by e.g. only storing the partial zobrist
 * hash, given a large enough cache. Unfortunately, during earlier experinents
 * with the naive hashing, the extra arithmetic was shown negatively impact
 * performance, so we stick with the 4 entries per bucket for now.
 * Another nice benefit with the 4 entries:
 * 64 bytes / 4 = 16 bytes, or two uint64_t which makes handling the data
 * extremely easy.
 *
 * Regarding replacement:
 * The closer to the root node a cache hit occurs, the the bigger the benefit,
 * so we definitely want to replace entries with a low remaining depth over
 * ones with a high remaining depth.
 * Likewise, we want to keep fresh entries from a recent search compared to
 * entries from a previous search which likely involves positions no longer
 * reachable, e.g. because some pieces got captured since.
 * Last but not least, entry with the same zobrist hash needs to be over-
 * written regarless of any other condition for three reasons: First, if there
 * already was an entry with the same zobrist hash, it means that the entry
 * was useless for us to begin with, else it would have produced a cut off and
 * we would not need to store an entry with the same key. Second, if there are
 * multiple entries with the same key, which one would take precedence? Kiss
 * principle, so first one. So why keep others at all? Last but not least,
 * there might be type-1 collisions, so it is desired that we replace such
 * entries as soon as possible.
 *
 * So here goes the replacement algorithm:
 * If there is entry with same key:
 *   Replace.
 * Else:
 *   Sort entries first by descending age, then by ascending remaining depth
 *   Replace first sorted entry
 *
 * Implementation specifics:
 * Hashing is implemented lockless using the xor technique described in
 * Hyatt's paper on lockless transposition tables.
 */

typedef unsigned long long hash_key;

struct move;
struct entry;

namespace score_type {
enum type {
	none,
	exact,
	lower_bound,
	upper_bound
};
}

class hash
{
public:
	hash();
	~hash();

	class stats
	{
public:
		stats()
			: entries()
			, hits()
			, best_move()
			, misses()
			, index_collisions()
		{}

		unsigned long long entries;
		unsigned long long hits;
		unsigned long long best_move;
		unsigned long long misses;
		unsigned long long index_collisions;
	};

	stats get_stats( bool reset );

	// max_size is in megabytes
	bool init( unsigned int max_size );

	// Returns true on hit, caller should return eval.
	// If it returns false and best_move.other==1, there is a best move.
	score_type::type lookup( hash_key key, color::type c, unsigned char depth, unsigned char ply, short alpha, short beta, short& eval, move& best_move );

	void store( hash_key key, color::type c, unsigned char depth, unsigned char ply, short eval, short alpha, short beta, move const& best_move, unsigned char clock );

	void free_hash();

	void clear_data();

	uint64_t max_hash_entry_count() const;

private:
	stats stats_;

	hash_key size_;
	hash_key bucket_count_;
	entry* data_;
	char* memory_;
};

extern hash transposition_table;

#endif //__HASH_H__
