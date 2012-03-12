#ifndef __UTIL_H__
#define __UTIL_H__

#include "chess.hpp"
#include "detect_check.hpp"

#include <string>

struct move_info;

bool validate_move( position const& p, move const& m, color::type c );

bool validate_move( move const& m, move_info const* begin, move_info const* end );

bool parse_move( position const& p, color::type c, std::string const& line, move& m, bool print_errors = true );

// E.g. O-O, Na3xf6, b2-b4
std::string move_to_string( move const& m, bool padding = true );

// E.g. c4d6, e1g1, e7e8q
std::string move_to_long_algebraic( move const& m );

void init_board( position& p );
void init_bitboards( position& p );
void init_material( position& p );

void apply_move( position& p, move const& m, color::type c );
bool apply_hash_move( position& p, move const& m, color::type c, check_map const& check );
void apply_move( position& p, move_info const& m, color::type c );

// Checks if the given move is legal in the given position.
// Precondition: There must be some position where the move is legal, else the result is undefined.
//				 e.g. is_valid_move might return true on Na1b1
bool is_valid_move( position const& p, color::type c, move const& m, check_map const& check );

std::string board_to_string( position const& p );

// Numbered square from 0 - 63
pieces::type get_piece_on_square( position const& p, color::type c, uint64_t square );

#endif
