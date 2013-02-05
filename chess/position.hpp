#ifndef __POSITION_H__
#define __POSITION_H__

#include "util/platform.hpp"
#include "definitions.hpp"
#include "score.hpp"

class move;

/*
 * Class to represent a complete chess position, sans repetition history.
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
	// Otherwise the enpassant square.
	unsigned char can_en_passant;

	// After setting bitboards, castling rights and en-passant square,
	// call this function to 
	void update_derived();

	void clear_bitboards();

	uint64_t pawn_hash;

	score material[2];

	inline color::type self() const { return c; }
	inline color::type other() const { return static_cast<color::type>(1-c); }
	inline bool white() const { return c == color::white; }
	inline bool black() const { return c == color::black; }

	// Material and pst, nothing else.
	score base_eval;

	bitboard bitboards[2];

	int king_pos[2];

	bool is_occupied_square( uint64_t square ) const;
	uint64_t get_occupancy( uint64_t mask ) const;

	color::type c;

	// Resets position to initial starting position
	void reset();

	bool verify() const;
	bool verify( std::string& error ) const;

	unsigned char do_null_move();

	pieces::type get_piece( uint64_t square ) const { return ::get_piece(get_piece_with_color(square)); }
	pieces_with_color::type get_piece_with_color( uint64_t square ) const;

	// Only call with valid moves
	pieces::type get_piece( move const& m ) const;
	pieces::type get_captured_piece( move const& m ) const;
	pieces_with_color::type get_piece_with_color( move const& m ) const;
	pieces_with_color::type get_captured_piece_with_color( move const& m ) const;

	pieces_with_color::type board[64];

	// Used as key to switch between endgame evaluations
	uint64_t piece_sum;

private:
	void init_bitboards();
	void init_board();
	void init_pawn_hash();
	void init_material();
	void init_eval();
	void init_piece_sum();
};

#endif
