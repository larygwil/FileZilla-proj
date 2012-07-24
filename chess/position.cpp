#include "position.hpp"
#include "chess.hpp"
#include "eval_values.hpp"
#include "tables.hpp"
#include "util.hpp"

#include <iostream>

position::position()
{
	reset();
}


void position::reset()
{
	castle[color::white] = 0x3;
	castle[color::black] = 0x3;
	
	can_en_passant = 0;

	c = color::white;

	init_bitboards();

	update_derived();
}


void position::update_derived()
{
	for( int i = 0; i < 2; ++i ) {
		bitboards[i].b[bb_type::all_pieces] = bitboards[i].b[bb_type::pawns] | bitboards[i].b[bb_type::knights] | bitboards[i].b[bb_type::bishops] | bitboards[i].b[bb_type::rooks] | bitboards[i].b[bb_type::queens] | bitboards[i].b[bb_type::king];

		bitboards[i].b[bb_type::pawn_control] = 0;
		uint64_t pawns = bitboards[i].b[bb_type::pawns];
		while( pawns ) {
			uint64_t pawn = bitscan_unset( pawns );
			bitboards[i].b[bb_type::pawn_control] |= pawn_control[i][pawn];
		}

		king_pos[i] = static_cast<int>(bitscan( bitboards[i].b[bb_type::king] ));
	}

	init_material();
	init_eval();
	init_pawn_hash();
}


void position::init_material()
{
	material[0] = score();
	material[1] = score();

	for( int i = 0; i < 2; ++i ) {
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			pieces::type b = get_piece_on_square( *this, static_cast<color::type>(i), pi );

			if( b != pieces::king ) {
				material[i] += eval_values::material_values[b];
			}
		}
	}
}


void position::init_eval()
{
	base_eval = material[0] - material[1];

	for( int i = 0; i < 2; ++i ) {
		score side;
		for( unsigned int sq = 0; sq < 64; ++sq ) {
			pieces::type pi = get_piece_on_square( *this, static_cast<color::type>(i), sq );

			side += pst[i][pi][sq];
		}

		if( i ) {
			base_eval -= side;
		}
		else {
			base_eval += side;
		}
	}
}


void position::clear_bitboards()
{
	memset( bitboards, 0, sizeof(bitboard) * 2 );
}


void position::init_bitboards()
{
	clear_bitboards();

	bitboards[color::white].b[bb_type::pawns]   = 0xff00ull;
	bitboards[color::white].b[bb_type::knights] = (1ull << 1) + (1ull << 6);
	bitboards[color::white].b[bb_type::bishops] = (1ull << 2) + (1ull << 5);
	bitboards[color::white].b[bb_type::rooks]   = 1ull + (1ull << 7);
	bitboards[color::white].b[bb_type::queens]  = 1ull << 3;
	bitboards[color::white].b[bb_type::king]    = 1ull << 4;

	bitboards[color::black].b[bb_type::pawns]   = 0xff000000000000ull;
	bitboards[color::black].b[bb_type::knights] = ((1ull << 1) + (1ull << 6)) << (7*8);
	bitboards[color::black].b[bb_type::bishops] = ((1ull << 2) + (1ull << 5)) << (7*8);
	bitboards[color::black].b[bb_type::rooks]   = (1ull + (1ull << 7)) << (7*8);
	bitboards[color::black].b[bb_type::queens]  = (1ull << 3) << (7*8);
	bitboards[color::black].b[bb_type::king]    = (1ull << 4) << (7*8);
}


void position::do_null_move()
{
	c = static_cast<color::type>(1-c);
}


bool position::verify() const
{
	std::string error;
	bool ret = verify( error );
	if( !ret ) {
		std::cerr << error << std::endl;
	}

	return ret;
}

bool position::verify( std::string& error ) const
{
	position p2 = *this;
	p2.update_derived();

	if( popcount(bitboards[color::white].b[bb_type::king]) != 1 ) {
		error = "White king count is not exactly one";
		return false;
	}
	if( popcount(bitboards[color::black].b[bb_type::king]) != 1 ) {
		error = "Black king count is not exactly one";
		return false;
	}
	if( bitscan( bitboards[color::white].b[bb_type::king]) != king_pos[color::white] ) {
		error = "Internal error: White king position does not match king bitboard";
		return false;
	}
	if( bitscan( bitboards[color::black].b[bb_type::king]) != king_pos[color::black] ) {
		error = "Internal error: Black king position does not match king bitboard";
		return false;
	}
	if( possible_king_moves[king_pos[color::white]] & bitboards[color::black].b[bb_type::king] ) {
		error = "Kings next to each other are not allowed";
		return false;
	}
	if( base_eval != p2.base_eval ) {
		error = "Internal error: Base evaluation mismatch";
		return false;
	}
	if( pawn_hash != p2.pawn_hash ) {
		error = "Internal error: Pawn hash mismatch";
		return false;
	}
	if( material[0] != p2.material[0] || material[1] != p2.material[1] ) {
		error = "Internal error: Material mismatch";
		return false;
	}
	for( int c = 0; c < 1; ++c ) {
		if( bitboards[c].b[bb_type::all_pieces] != p2.bitboards[c].b[bb_type::all_pieces] ) {
			error = "Internal error: Bitboard error: Wrong all pieces";
			return false;
		}
		if( bitboards[c].b[bb_type::pawn_control] != p2.bitboards[c].b[bb_type::pawn_control] ) {
			error = "Internal error: Pawn control bitboard incorrect";
			return false;
		}
	}
	if( bitboards[color::white].b[bb_type::all_pieces] & bitboards[color::black].b[bb_type::all_pieces] ) {
		error = "White and black pieces not disjunct";
		return false;
	}
	if( castle[color::white] & castles::kingside ) {
		if( king_pos[color::white] != 4 || !(bitboards[color::white].b[bb_type::rooks] & (1ull << 7)) ) {
			error = "White's kingside castling right is not correct";
			return false;
		}
	}
	if( castle[color::white] & castles::queenside ) {
		if( king_pos[color::white] != 4 || !(bitboards[color::white].b[bb_type::rooks] & 1ull) ) {
			error = "White's queenside castling right is not correct";
			return false;
		}
	}
	if( castle[color::black] & castles::kingside ) {
		if( king_pos[color::black] != 60 || !(bitboards[color::black].b[bb_type::rooks] & (1ull << 63)) ) {
			error = "Black's kingside castling right is not correct";
			return false;
		}
	}
	if( castle[color::black] & castles::queenside ) {
		if( king_pos[color::black] != 60 || !(bitboards[color::black].b[bb_type::rooks] & (1ull << 56)) ) {
			error = "Black's queenside castling right is not correct";
			return false;
		}
	}

	for( int i = 0; i < 2; ++i ) {
		uint64_t all = 0;
		for( int pi = bb_type::pawns; pi <= bb_type::king; ++pi ) {
			all += popcount(bitboards[i].b[pi]);
		}
		if( all != popcount(bitboards[i].b[bb_type::all_pieces]) ) {
			error = "Internal error: Bitboards for difference pieces not disjunct";
			return false;
		}
	}

	if( can_en_passant ) {
		if( self() == color::black ) {
			if( can_en_passant < 16 || can_en_passant >= 24 ) {
				error = "Enpassant square incorrect";
				return false;
			}
			uint64_t occ = 1ull << (can_en_passant + 8);
			uint64_t free = (1ull << can_en_passant) | (1ull << (can_en_passant - 8));

			if( (bitboards[color::white].b[bb_type::all_pieces] | bitboards[color::black].b[bb_type::all_pieces]) & free ) {
				error = "Incorrect enpassant square, pawn could not have made double-move.";
				return false;
			}
			if( !(bitboards[other()].b[bb_type::pawns] & occ) ) {
				error = "Incorrect enpassant square, there is no corresponding pawn.";
				return false;
			}
		}
		else {
			if( can_en_passant < 40 || can_en_passant >= 48 ) {
				error = "Enpassant square incorrect";
				return false;
			}
			uint64_t occ = 1ull << (can_en_passant - 8);
			uint64_t free = (1ull << can_en_passant) | (1ull << (can_en_passant + 8));

			if( (bitboards[color::white].b[bb_type::all_pieces] | bitboards[color::black].b[bb_type::all_pieces]) & free ) {
				error = "Incorrect enpassant square, pawn could not have made double-move.";
				return false;
			}
			if( !(bitboards[other()].b[bb_type::pawns] & occ) ) {
				error = "Incorrect enpassant square, there is no corresponding pawn.";
				return false;
			}
		}
	}

	if( detect_check( *this, other() ) ) {
		error = "The side not to move is in check.";
		return false;
	}
	
	return true;
}
