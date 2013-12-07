#ifndef __POSITION_H__
#define __POSITION_H__

#include "util/platform.hpp"
#include "definitions.hpp"
#include "score.hpp"

class move;

namespace square {
enum type {
	a1, b1, c1, d1, e1, f1, g1, h1,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a8, b8, c8, d8, e8, f8, g8, h8
};
}

extern uint64_t const light_squares;
bool is_light( square::type sq );
bool is_light_mask( uint64_t mask );

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

	// At most two bits are set, each bit indicates the file of the rook that can castle.
	unsigned char castle[2];

	// 0 if en-passant not possible.
	// Otherwise the enpassant square.
	unsigned char can_en_passant;

	// After setting bitboards, castling rights and en-passant square,
	// call this function to update all derived values, e.g. material or
	// the pawn hash.
	void update_derived();

	void clear_bitboards();

	uint64_t pawn_hash;

	score material[2];

	inline color::type self() const { return c; }
	inline color::type other() const { return ::other(c); }
	inline bool white() const { return c == color::white; }
	inline bool black() const { return c == color::black; }

	// Material and pst, nothing else.
	score base_eval;

	bitboard bitboards[2][bb_type::value_max];

	square::type king_pos[2];

	bool is_occupied_square( uint64_t square ) const;
	uint64_t get_occupancy( uint64_t mask = 0xffffffffffffffffull ) const;

	color::type c;

	// Resets position to initial starting position
	void reset();

	bool verify() const;
	bool verify( std::string& error ) const;
	void verify_abort() const;

	bool verify_castling( std::string& error, bool allow_frc ) const;

	unsigned char do_null_move( unsigned char old_enpassant = 0 );

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

	unsigned int halfmoves_since_pawnmove_or_capture;

	uint64_t hash_;

	uint64_t init_hash() const;
	void init_material();
	void init_eval();
private:
	void init_bitboards();
	void init_board();
	void init_pawn_hash();
	void init_piece_sum();
};

#endif
