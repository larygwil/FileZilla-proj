#ifndef __POSITION_H__
#define __POSITION_H__

#include "platform.hpp"
#include "definitions.hpp"
#include "score.hpp"

/*
 * Class to represent a complete chess position, sans repetition history.
 *
 * Default constructor leaves board uninitialized. Call init() to reset board to start position.
 *
 * For performance reasons, position contains some redundancies. Call update_derive to synchronize all derived values
 * from the bitboards.
 */
class position
{
public:
	position();

	// Bit 0: can castle kingside
	// Bit 1: can castle queenside
	// Bit 2: has castled
	short castle[2];

	// 0 if en-passant not possible.
	// Otherwise board index of last-moved pawn that can be en-passanted in
	// lower 6 bits, 7th bit color of pawn that is en-passantable.
	unsigned char can_en_passant;

	// After setting bitboards, castling rights and en-passant square,
	// call this function to 
	void update_derived();

	void clear_bitboards();

	uint64_t pawn_hash;

	score material[2];

	// Material and pst, nothing else.
	score base_eval;

	bitboard bitboards[2];

	int king_pos[2];

	bool is_occupied_square( uint64_t square ) const;
	uint64_t get_occupancy( uint64_t mask ) const;

	// Resets position to initial starting position
	void reset();

	bool verify() const;

private:
	void init_bitboards();
	void init_pawn_hash();
	void init_material();
	void init_eval();
};

#endif
