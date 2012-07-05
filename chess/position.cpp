#include "position.hpp"
#include "chess.hpp"
#include "eval_values.hpp"
#include "tables.hpp"
#include "util.hpp"

position::position()
{
	reset();
}


void position::reset()
{
	for( unsigned int c = 0; c <= 1; ++c ) {
		castle[c] = 0x3;
	}

	can_en_passant = 0;

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

	for( int c = 0; c < 2; ++c ) {
		for( unsigned int pi = 0; pi < 64; ++pi ) {
			pieces::type b = get_piece_on_square( *this, static_cast<color::type>(c), pi );

			if( b != pieces::king ) {
				material[c] += eval_values::material_values[b];
			}
		}
	}
}


void position::init_eval()
{
	base_eval = material[0] - material[1];

	for( int c = 0; c < 2; ++c ) {
		score side;
		for( unsigned int sq = 0; sq < 64; ++sq ) {
			pieces::type pi = get_piece_on_square( *this, static_cast<color::type>(c), sq );

			side += pst[c][pi][sq];
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
