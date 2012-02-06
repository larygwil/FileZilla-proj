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

// E.g. c4d6
std::string move_to_source_target_string( move const& m );

void init_board( position& p );
void init_bitboards( position& p );
void init_material( position& p );

bool apply_move( position& p, move const& m, color::type c );
bool apply_hash_move( position& p, move const& m, color::type c, check_map const& check );
bool apply_move( position& p, move_info const& m, color::type c );

void init_random( unsigned long long seed );

void push_rng_state();
void pop_rng_state();

unsigned char get_random_unsigned_char();

unsigned long long get_random_unsigned_long_long();

std::string board_to_string( position const& p );

// Numbered square from 0 - 63
pieces::type get_piece_on_square( position const& p, color::type c, unsigned long long square );

#endif
