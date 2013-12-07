#include "position.hpp"
#include "chess.hpp"
#include "config.hpp"
#include "eval_values.hpp"
#include "tables.hpp"
#include "util.hpp"
#include "zobrist.hpp"

#include <iostream>

extern uint64_t const light_squares = 0x55aa55aa55aa55aaull;

bool is_light( square::type sq )
{
	return is_light_mask( 1ull << sq );
}

bool is_light_mask( uint64_t mask )
{
	return (mask & light_squares) == mask;
}

position::position()
{
	reset();
}


void position::reset()
{
	castle[color::white] = 0x81;
	castle[color::black] = 0x81;
	
	can_en_passant = 0;

	c = color::white;

	halfmoves_since_pawnmove_or_capture = 0;

	init_bitboards();

	update_derived();
}


void position::update_derived()
{
	for( int i = 0; i < 2; ++i ) {
		bitboards[i][bb_type::all_pieces] = bitboards[i][bb_type::pawns] | bitboards[i][bb_type::knights] | bitboards[i][bb_type::bishops] | bitboards[i][bb_type::rooks] | bitboards[i][bb_type::queens] | bitboards[i][bb_type::king];

		bitboards[i][bb_type::pawn_control] = 0;
		uint64_t pawns = bitboards[i][bb_type::pawns];
		while( pawns ) {
			uint64_t pawn = bitscan_unset( pawns );
			bitboards[i][bb_type::pawn_control] |= pawn_control[i][pawn];
		}

		king_pos[i] = static_cast<square::type>(bitscan( bitboards[i][bb_type::king] ));
	}

	init_board();
	init_material();
	init_eval();
	init_pawn_hash();
	init_piece_sum();
	hash_ = init_hash();
}


void position::init_piece_sum()
{
	piece_sum = 0;
	for( int c = 0; c < 2; ++c ) {
		for( int piece = pieces::pawn; piece < pieces::king; ++piece ) {
			uint64_t count = popcount( bitboards[c][piece] );
			piece_sum |= count << ((piece - 1 + (c ? 5 : 0)) * 6);
		}
	}
}


void position::init_material()
{
	material[0] = score();
	material[1] = score();

	for( unsigned int c = 0; c < 2; ++c ) {
		uint64_t all = bitboards[c][bb_type::knights] | bitboards[c][bb_type::bishops] | bitboards[c][bb_type::rooks] | bitboards[c][bb_type::queens];
		while( all ) {
			uint64_t sq = bitscan_unset(all);

			pieces::type pi = get_piece( sq );

			material[c] += eval_values::material_values[pi];
		}
	}
}


void position::init_eval()
{
	base_eval = material[0] - material[1] +
		eval_values::material_values[pieces::pawn] * static_cast<short>(popcount(bitboards[0][bb_type::pawns])) -
		eval_values::material_values[pieces::pawn] * static_cast<short>(popcount(bitboards[1][bb_type::pawns]));

	for( unsigned int c = 0; c < 2; ++c ) {
		score side;

		uint64_t all = bitboards[c][bb_type::all_pieces];
		while( all ) {
			uint64_t sq = bitscan_unset(all);

			pieces::type pi = get_piece( sq );

			side += pst(static_cast<color::type>(c), pi, sq);
		}

		if( c ) {
			base_eval -= side;
		}
		else {
			base_eval += side;
		}
	}
}


void position::clear_bitboards()
{
	memset( bitboards, 0, sizeof(bitboards) );
}


void position::init_bitboards()
{
	clear_bitboards();

	bitboards[color::white][bb_type::pawns]   = 0xff00ull;
	bitboards[color::white][bb_type::knights] = (1ull << 1) + (1ull << 6);
	bitboards[color::white][bb_type::bishops] = (1ull << 2) + (1ull << 5);
	bitboards[color::white][bb_type::rooks]   = 1ull + (1ull << 7);
	bitboards[color::white][bb_type::queens]  = 1ull << 3;
	bitboards[color::white][bb_type::king]    = 1ull << 4;

	bitboards[color::black][bb_type::pawns]   = 0xff000000000000ull;
	bitboards[color::black][bb_type::knights] = ((1ull << 1) + (1ull << 6)) << (7*8);
	bitboards[color::black][bb_type::bishops] = ((1ull << 2) + (1ull << 5)) << (7*8);
	bitboards[color::black][bb_type::rooks]   = (1ull + (1ull << 7)) << (7*8);
	bitboards[color::black][bb_type::queens]  = (1ull << 3) << (7*8);
	bitboards[color::black][bb_type::king]    = (1ull << 4) << (7*8);
}

void position::init_board()
{
	for( int sq = 0; sq < 64; ++sq ) {
		board[sq] = pieces_with_color::none;
		for( int c = 0; c < 2; ++c ) {
			for( int type = bb_type::pawns; type <= bb_type::king; ++type) {
				if( bitboards[c][type] & (1ull << sq)) {
					board[sq] = static_cast<pieces_with_color::type>(c * 8 + type);
				}
			}
		}
	}
}

unsigned char position::do_null_move( unsigned char old_enpassant )
{
	c = ::other(c);

	unsigned char enpassant = can_en_passant;
	can_en_passant = old_enpassant;

	hash_ = ~(hash_ ^ get_enpassant_hash( enpassant ) );
	hash_ ^= get_enpassant_hash( old_enpassant );

	return enpassant;
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

	if( popcount(bitboards[color::white][bb_type::king]) != 1 ) {
		error = "White king count is not exactly one";
		return false;
	}
	if( popcount(bitboards[color::black][bb_type::king]) != 1 ) {
		error = "Black king count is not exactly one";
		return false;
	}
	if( bitscan( bitboards[color::white][bb_type::king]) != king_pos[color::white] ) {
		error = "Internal error: White king position does not match king bitboard";
		return false;
	}
	if( bitscan( bitboards[color::black][bb_type::king]) != king_pos[color::black] ) {
		error = "Internal error: Black king position does not match king bitboard";
		return false;
	}
	if( possible_king_moves[king_pos[color::white]] & bitboards[color::black][bb_type::king] ) {
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
		if( bitboards[c][bb_type::all_pieces] != p2.bitboards[c][bb_type::all_pieces] ) {
			error = "Internal error: Bitboard error: Wrong all pieces";
			return false;
		}
		if( bitboards[c][bb_type::pawn_control] != p2.bitboards[c][bb_type::pawn_control] ) {
			error = "Internal error: Pawn control bitboard incorrect";
			return false;
		}
	}
	if( bitboards[color::white][bb_type::all_pieces] & bitboards[color::black][bb_type::all_pieces] ) {
		error = "White and black pieces not disjunct";
		return false;
	}

	for( int i = 0; i < 2; ++i ) {
		uint64_t all = 0;
		for( int pi = bb_type::pawns; pi <= bb_type::king; ++pi ) {
			all += popcount(bitboards[i][pi]);
		}
		if( all != popcount(bitboards[i][bb_type::all_pieces]) ) {
			error = "Internal error: Bitboards for difference pieces not disjunct";
			return false;
		}
	}

	if( !verify_castling( error, true ) ) {
		return false;
	}

	if( can_en_passant ) {
		if( self() == color::black ) {
			if( can_en_passant < 16 || can_en_passant >= 24 ) {
				error = "Enpassant square incorrect";
				return false;
			}
			uint64_t occ = 1ull << (can_en_passant + 8);
			uint64_t free = (1ull << can_en_passant) | (1ull << (can_en_passant - 8));

			if( (bitboards[color::white][bb_type::all_pieces] | bitboards[color::black][bb_type::all_pieces]) & free ) {
				error = "Incorrect enpassant square, pawn could not have made double-move.";
				return false;
			}
			if( !(bitboards[other()][bb_type::pawns] & occ) ) {
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

			if( (bitboards[color::white][bb_type::all_pieces] | bitboards[color::black][bb_type::all_pieces]) & free ) {
				error = "Incorrect enpassant square, pawn could not have made double-move.";
				return false;
			}
			if( !(bitboards[other()][bb_type::pawns] & occ) ) {
				error = "Incorrect enpassant square, there is no corresponding pawn.";
				return false;
			}
		}
	}

	if( detect_check( *this, other() ) ) {
		error = "The side not to move is in check.";
		return false;
	}

	for( int sq = 0; sq < 64; ++sq ) {
		pieces_with_color::type pwc = board[sq];
		color::type c = get_color(pwc);
		pieces::type pi = ::get_piece(pwc);
		if( pi == pieces::none ) {
			if( bitboards[c][bb_type::all_pieces] & (1ull << sq) ) {
				error = "Board contains non-empty square where it should be empty.";
				return false;
			}
		}
		else if( !(bitboards[c][pi] & (1ull << sq) ) ) {
			error = "Board contains wrong data.";
			return false;
		}
	}


	uint64_t ver_piece_sum = 0;
	for( int c = 0; c < 2; ++c ) {
		for( int piece = pieces::pawn; piece < pieces::king; ++piece ) {
			uint64_t count = popcount( bitboards[c][piece] );
			ver_piece_sum |= count << ((piece - 1 + (c ? 5 : 0)) * 6);
		}
	}
	if( piece_sum != ver_piece_sum ) {
		error = "Piece sum mismatch.";
		return false;
	}

	uint64_t hash = init_hash();
	if( hash != hash_ ) {
		error = "Hash mismatch";
		return false;
	}

	return true;
}


void position::verify_abort() const
{
	if( !verify() ) {
		abort();
	}
}


pieces_with_color::type position::get_piece_with_color( uint64_t square ) const
{
	return board[square];
}

pieces::type position::get_piece( move const& m ) const
{
	return get_piece( m.source() );
}

pieces::type position::get_captured_piece( move const& m ) const
{
	pieces::type ret;
	if( m.enpassant() ) {
		ret = pieces::pawn;
	}
	else {
		ret = get_piece( m.target() );
	}
	return ret;
}

pieces_with_color::type position::get_piece_with_color( move const& m ) const
{
	return get_piece_with_color( m.source() );
}

pieces_with_color::type position::get_captured_piece_with_color( move const& m ) const
{
	pieces_with_color::type ret;
	if( m.enpassant() ) {
		ret = white() ? pieces_with_color::black_pawn : pieces_with_color::white_pawn;
	}
	else {
		ret = get_piece_with_color( m.target() );
	}
	return ret;
}

uint64_t position::init_hash() const
{
	uint64_t hash = 0;

	for( unsigned int c = 0; c < 2; ++c ) {
		uint64_t pieces = bitboards[c][bb_type::all_pieces];
		while( pieces ) {
			uint64_t piece = bitscan_unset( pieces );

			hash ^= get_piece_hash( get_piece( piece ), static_cast<color::type>(c), piece );
		}

		hash ^= zobrist::castle[c][castle[c]];
	}

	hash ^= zobrist::enpassant[can_en_passant];

	if( !white() ) {
		hash = ~hash;
	}

	return hash;
}


bool position::verify_castling( std::string& error, bool allow_frc ) const
{
	for( int i = 0; i < 2; ++i ) {
		uint64_t s = castle[i];
		if( s ) {
			if( popcount(castle[i]) > 2 ) {
				error = color::to_string(static_cast<color::type>(i)) + "'s castling rights include too many sides";
				return false;
			}

			if( popcount(castle[i]) == 2 ) {
				uint64_t queenside = bitscan(castle[i]);
				uint64_t kingside = bitscan_reverse(castle[i]);
				uint64_t king = king_pos[i] % 8;
				if( queenside > king || kingside < king ) {
					error = color::to_string(static_cast<color::type>(i)) + "'s castling rights do not have king in center";
					return false;
				}
			}

			while( s ) {
				uint64_t r = 1ull << bitscan_unset(s);
				if( i == color::black ) {
					r <<= (7 * 8);
				}
				if( !(bitboards[i][bb_type::rooks] & r) ) {
					error = color::to_string(static_cast<color::type>(i)) + "'s castling rights do not match the rook positions";
					return false;
				}
			}
			
			if( !allow_frc ) {
				if( king_pos[i] != (i ? 60 : 4) ) {
					error = color::to_string(static_cast<color::type>(i)) + "'s castling rights do not match the king positions";
					return false;
				}
				if( castle[i] & 0x7e ) {
					error = color::to_string(static_cast<color::type>(i)) + "'s castling rights on rooks other than A and H file";
					return false;
				}
			}
			else {
				uint64_t castle_row = 0x7eull << (i ? 56 : 0);
				if( !((1ull << king_pos[i]) & castle_row) ) {
					error = color::to_string(static_cast<color::type>(i)) + "'s castling rights do not match the king positions";
					return false;
				}
			}
		}
	}

	return true;
}
