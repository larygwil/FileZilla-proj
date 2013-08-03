#ifndef OCTOCHESS_CONTEXT_HEADER
#define OCTOCHESS_CONTEXT_HEADER

#include "config.hpp"
#include "hash.hpp"
#include "pawn_structure_hash_table.hpp"

class context
{
public:
	config conf_;
	hash tt_;
	pawn_structure_hash_table pawn_tt_;
};

#endif