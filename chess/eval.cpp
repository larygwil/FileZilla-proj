#include "eval.hpp"
#include "assert.hpp"
#include "util.hpp"
#include "pawn_structure_hash_table.hpp"
#include "zobrist.hpp"

#include <iostream>

namespace special_values {
enum type
{
	double_bishop = 25,
	castled = 25,
	doubled_pawn = -50,
	passed_pawn = 30,
	connected_pawn = 15
};
}

namespace {
unsigned char const pawn_values[2][8][8] =
	{
		{
			{ 0, 105, 105, 100,  97, 106, 120, 0 },
			{ 0, 110,  95, 100, 103, 112, 127, 0 },
			{ 0, 110,  90, 110, 117, 125, 135, 0 },
			{ 0,  80, 100, 120, 127, 140, 150, 0 },
			{ 0,  80, 100, 120, 127, 140, 150, 0 },
			{ 0, 110,  90, 110, 117, 125, 135, 0 },
			{ 0, 110,  95, 100, 103, 112, 127, 0 },
			{ 0, 105, 105, 100, 100, 106, 120, 0 }
		},
		{
			{ 0, 120, 106,  97, 100, 105, 105, 0 },
			{ 0, 127, 112, 103, 100,  95, 110, 0 },
			{ 0, 135, 125, 117, 110,  90, 110, 0 },
			{ 0, 150, 140, 127, 120, 100,  80, 0 },
			{ 0, 150, 140, 127, 120, 100,  80, 0 },
			{ 0, 135, 125, 117, 110,  90, 110, 0 },
			{ 0, 127, 112, 103, 100,  95, 110, 0 },
			{ 0, 120, 106, 100, 100, 105, 105, 0 }
		}

	};

signed short const queen_values[2][8][8] = {
	{
		{ 880, 890, 890, 895, 900, 890, 890, 880 },
		{ 890, 900, 905, 900, 900, 900, 900, 890 },
		{ 890, 905, 905, 905, 905, 905, 900, 890 },
		{ 895, 900, 905, 905, 905, 905, 900, 895 },
		{ 895, 900, 905, 905, 905, 905, 900, 895 },
		{ 890, 900, 905, 905, 905, 905, 900, 890 },
		{ 890, 900, 900, 900, 900, 900, 900, 890 },
		{ 880, 890, 890, 895, 895, 890, 890, 880 }
	},
	{
		{ 880, 890, 890, 895, 900, 890, 890, 880 },
		{ 890, 900, 900, 900, 900, 900, 900, 890 },
		{ 890, 900, 905, 905, 905, 905, 900, 890 },
		{ 895, 900, 905, 905, 905, 905, 900, 895 },
		{ 895, 900, 905, 905, 905, 905, 900, 895 },
		{ 890, 900, 905, 905, 905, 905, 905, 890 },
		{ 890, 900, 900, 900, 900, 905, 900, 890 },
		{ 880, 890, 890, 895, 895, 890, 890, 880 }
	}
};

signed short const rook_values[2][8][8] = {
	{
		{ 500, 495, 495, 495, 495, 495, 505, 500 },
		{ 500, 500, 500, 500, 500, 500, 510, 500 },
		{ 500, 500, 500, 500, 500, 500, 510, 500 },
		{ 505, 500, 500, 500, 500, 500, 510, 500 },
		{ 505, 500, 500, 500, 500, 500, 510, 500 },
		{ 500, 500, 500, 500, 500, 500, 510, 500 },
		{ 500, 500, 500, 500, 500, 500, 510, 500 },
		{ 500, 495, 495, 495, 495, 495, 505, 500 }
	},
	{
		{ 500, 505, 495, 495, 495, 495, 495, 500 },
		{ 500, 510, 500, 500, 500, 500, 500, 500 },
		{ 500, 510, 500, 500, 500, 500, 500, 500 },
		{ 500, 510, 500, 500, 500, 500, 500, 505 },
		{ 500, 510, 500, 500, 500, 500, 500, 505 },
		{ 500, 510, 500, 500, 500, 500, 500, 500 },
		{ 500, 510, 500, 500, 500, 500, 500, 500 },
		{ 500, 505, 495, 495, 495, 495, 495, 500 }
	}
};

signed short const knight_values[2][8][8] = {
	{
		{ 260, 270, 280, 280, 280, 280, 270, 260 },
		{ 270, 290, 305, 310, 305, 310, 290, 270 },
		{ 280, 310, 310, 315, 315, 310, 310, 280 },
		{ 280, 305, 315, 320, 320, 315, 310, 280 },
		{ 280, 305, 315, 320, 320, 315, 310, 280 },
		{ 280, 310, 310, 315, 315, 310, 310, 280 },
		{ 270, 290, 305, 310, 305, 310, 290, 270 },
		{ 260, 270, 280, 280, 280, 280, 270, 260 }
	},
	{
		{ 260, 270, 280, 280, 280, 280, 270, 260 },
		{ 270, 290, 310, 305, 310, 305, 290, 270 },
		{ 280, 310, 310, 315, 315, 310, 310, 280 },
		{ 280, 310, 315, 320, 320, 315, 305, 280 },
		{ 280, 310, 315, 320, 320, 315, 305, 280 },
		{ 280, 310, 310, 315, 315, 310, 310, 280 },
		{ 270, 290, 310, 305, 310, 305, 290, 270 },
		{ 260, 270, 280, 280, 280, 280, 270, 260 },
	}
};

signed short const bishop_values[2][8][8] = {
	{
		{ 300, 310, 310, 310, 310, 310, 310, 300 },
		{ 310, 325, 330, 320, 325, 320, 320, 310 },
		{ 310, 320, 330, 330, 325, 325, 320, 310 },
		{ 310, 320, 330, 330, 330, 330, 320, 310 },
		{ 310, 320, 330, 330, 330, 330, 320, 310 },
		{ 310, 320, 330, 330, 325, 325, 320, 310 },
		{ 310, 325, 330, 320, 325, 320, 320, 310 },
		{ 300, 310, 310, 310, 310, 310, 310, 300 }
	},
	{
		{ 300, 310, 310, 310, 310, 310, 310, 300 },
		{ 310, 320, 320, 325, 320, 330, 325, 310 },
		{ 310, 320, 325, 325, 330, 330, 320, 310 },
		{ 310, 320, 330, 330, 330, 330, 320, 310 },
		{ 310, 320, 330, 330, 330, 330, 320, 310 },
		{ 310, 320, 325, 325, 330, 330, 320, 310 },
		{ 310, 320, 320, 325, 320, 330, 325, 310 },
		{ 300, 310, 310, 310, 310, 310, 310, 300 }
	}
};

unsigned long long const passed_pawns[2][64] = {
{
		0x0303030303030300ull,
		0x0707070707070700ull,
		0x0e0e0e0e0e0e0e00ull,
		0x1c1c1c1c1c1c1c00ull,
		0x3838383838383800ull,
		0x7070707070707000ull,
		0xe0e0e0e0e0e0e000ull,
		0xc0c0c0c0c0c0c000ull,
		0x0303030303030000ull,
		0x0707070707070000ull,
		0x0e0e0e0e0e0e0000ull,
		0x1c1c1c1c1c1c0000ull,
		0x3838383838380000ull,
		0x7070707070700000ull,
		0xe0e0e0e0e0e00000ull,
		0xc0c0c0c0c0c00000ull,
		0x0303030303000000ull,
		0x0707070707000000ull,
		0x0e0e0e0e0e000000ull,
		0x1c1c1c1c1c000000ull,
		0x3838383838000000ull,
		0x7070707070000000ull,
		0xe0e0e0e0e0000000ull,
		0xc0c0c0c0c0000000ull,
		0x0303030300000000ull,
		0x0707070700000000ull,
		0x0e0e0e0e00000000ull,
		0x1c1c1c1c00000000ull,
		0x3838383800000000ull,
		0x7070707000000000ull,
		0xe0e0e0e000000000ull,
		0xc0c0c0c000000000ull,
		0x0303030000000000ull,
		0x0707070000000000ull,
		0x0e0e0e0000000000ull,
		0x1c1c1c0000000000ull,
		0x3838380000000000ull,
		0x7070700000000000ull,
		0xe0e0e00000000000ull,
		0xc0c0c00000000000ull,
		0x0303000000000000ull,
		0x0707000000000000ull,
		0x0e0e000000000000ull,
		0x1c1c000000000000ull,
		0x3838000000000000ull,
		0x7070000000000000ull,
		0xe0e0000000000000ull,
		0xc0c0000000000000ull,
		0x0300000000000000ull,
		0x0700000000000000ull,
		0x0e00000000000000ull,
		0x1c00000000000000ull,
		0x3800000000000000ull,
		0x7000000000000000ull,
		0xe000000000000000ull,
		0xc000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull
	},
{
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000003ull,
		0x0000000000000007ull,
		0x000000000000000eull,
		0x000000000000001cull,
		0x0000000000000038ull,
		0x0000000000000070ull,
		0x00000000000000e0ull,
		0x00000000000000c0ull,
		0x0000000000000303ull,
		0x0000000000000707ull,
		0x0000000000000e0eull,
		0x0000000000001c1cull,
		0x0000000000003838ull,
		0x0000000000007070ull,
		0x000000000000e0e0ull,
		0x000000000000c0c0ull,
		0x0000000000030303ull,
		0x0000000000070707ull,
		0x00000000000e0e0eull,
		0x00000000001c1c1cull,
		0x0000000000383838ull,
		0x0000000000707070ull,
		0x0000000000e0e0e0ull,
		0x0000000000c0c0c0ull,
		0x0000000003030303ull,
		0x0000000007070707ull,
		0x000000000e0e0e0eull,
		0x000000001c1c1c1cull,
		0x0000000038383838ull,
		0x0000000070707070ull,
		0x00000000e0e0e0e0ull,
		0x00000000c0c0c0c0ull,
		0x0000000303030303ull,
		0x0000000707070707ull,
		0x0000000e0e0e0e0eull,
		0x0000001c1c1c1c1cull,
		0x0000003838383838ull,
		0x0000007070707070ull,
		0x000000e0e0e0e0e0ull,
		0x000000c0c0c0c0c0ull,
		0x0000030303030303ull,
		0x0000070707070707ull,
		0x00000e0e0e0e0e0eull,
		0x00001c1c1c1c1c1cull,
		0x0000383838383838ull,
		0x0000707070707070ull,
		0x0000e0e0e0e0e0e0ull,
		0x0000c0c0c0c0c0c0ull,
		0x0003030303030303ull,
		0x0007070707070707ull,
		0x000e0e0e0e0e0e0eull,
		0x001c1c1c1c1c1c1cull,
		0x0038383838383838ull,
		0x0070707070707070ull,
		0x00e0e0e0e0e0e0e0ull,
		0x00c0c0c0c0c0c0c0ull
	}
};
unsigned long long const doubled_pawns[2][64] = {
{
		0x0101010101010100ull,
		0x0202020202020200ull,
		0x0404040404040400ull,
		0x0808080808080800ull,
		0x1010101010101000ull,
		0x2020202020202000ull,
		0x4040404040404000ull,
		0x8080808080808000ull,
		0x0101010101010000ull,
		0x0202020202020000ull,
		0x0404040404040000ull,
		0x0808080808080000ull,
		0x1010101010100000ull,
		0x2020202020200000ull,
		0x4040404040400000ull,
		0x8080808080800000ull,
		0x0101010101000000ull,
		0x0202020202000000ull,
		0x0404040404000000ull,
		0x0808080808000000ull,
		0x1010101010000000ull,
		0x2020202020000000ull,
		0x4040404040000000ull,
		0x8080808080000000ull,
		0x0101010100000000ull,
		0x0202020200000000ull,
		0x0404040400000000ull,
		0x0808080800000000ull,
		0x1010101000000000ull,
		0x2020202000000000ull,
		0x4040404000000000ull,
		0x8080808000000000ull,
		0x0101010000000000ull,
		0x0202020000000000ull,
		0x0404040000000000ull,
		0x0808080000000000ull,
		0x1010100000000000ull,
		0x2020200000000000ull,
		0x4040400000000000ull,
		0x8080800000000000ull,
		0x0101000000000000ull,
		0x0202000000000000ull,
		0x0404000000000000ull,
		0x0808000000000000ull,
		0x1010000000000000ull,
		0x2020000000000000ull,
		0x4040000000000000ull,
		0x8080000000000000ull,
		0x0100000000000000ull,
		0x0200000000000000ull,
		0x0400000000000000ull,
		0x0800000000000000ull,
		0x1000000000000000ull,
		0x2000000000000000ull,
		0x4000000000000000ull,
		0x8000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull
	},
{
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000000ull,
		0x0000000000000001ull,
		0x0000000000000002ull,
		0x0000000000000004ull,
		0x0000000000000008ull,
		0x0000000000000010ull,
		0x0000000000000020ull,
		0x0000000000000040ull,
		0x0000000000000080ull,
		0x0000000000000101ull,
		0x0000000000000202ull,
		0x0000000000000404ull,
		0x0000000000000808ull,
		0x0000000000001010ull,
		0x0000000000002020ull,
		0x0000000000004040ull,
		0x0000000000008080ull,
		0x0000000000010101ull,
		0x0000000000020202ull,
		0x0000000000040404ull,
		0x0000000000080808ull,
		0x0000000000101010ull,
		0x0000000000202020ull,
		0x0000000000404040ull,
		0x0000000000808080ull,
		0x0000000001010101ull,
		0x0000000002020202ull,
		0x0000000004040404ull,
		0x0000000008080808ull,
		0x0000000010101010ull,
		0x0000000020202020ull,
		0x0000000040404040ull,
		0x0000000080808080ull,
		0x0000000101010101ull,
		0x0000000202020202ull,
		0x0000000404040404ull,
		0x0000000808080808ull,
		0x0000001010101010ull,
		0x0000002020202020ull,
		0x0000004040404040ull,
		0x0000008080808080ull,
		0x0000010101010101ull,
		0x0000020202020202ull,
		0x0000040404040404ull,
		0x0000080808080808ull,
		0x0000101010101010ull,
		0x0000202020202020ull,
		0x0000404040404040ull,
		0x0000808080808080ull,
		0x0001010101010101ull,
		0x0002020202020202ull,
		0x0004040404040404ull,
		0x0008080808080808ull,
		0x0010101010101010ull,
		0x0020202020202020ull,
		0x0040404040404040ull,
		0x0080808080808080ull
	}
};
unsigned long long const connected_pawns[2][64] = {
{
		0x0000000000000202ull,
		0x0000000000000505ull,
		0x0000000000000a0aull,
		0x0000000000001414ull,
		0x0000000000002828ull,
		0x0000000000005050ull,
		0x000000000000a0a0ull,
		0x0000000000004040ull,
		0x0000000000020202ull,
		0x0000000000050505ull,
		0x00000000000a0a0aull,
		0x0000000000141414ull,
		0x0000000000282828ull,
		0x0000000000505050ull,
		0x0000000000a0a0a0ull,
		0x0000000000404040ull,
		0x0000000002020200ull,
		0x0000000005050500ull,
		0x000000000a0a0a00ull,
		0x0000000014141400ull,
		0x0000000028282800ull,
		0x0000000050505000ull,
		0x00000000a0a0a000ull,
		0x0000000040404000ull,
		0x0000000202020000ull,
		0x0000000505050000ull,
		0x0000000a0a0a0000ull,
		0x0000001414140000ull,
		0x0000002828280000ull,
		0x0000005050500000ull,
		0x000000a0a0a00000ull,
		0x0000004040400000ull,
		0x0000020202000000ull,
		0x0000050505000000ull,
		0x00000a0a0a000000ull,
		0x0000141414000000ull,
		0x0000282828000000ull,
		0x0000505050000000ull,
		0x0000a0a0a0000000ull,
		0x0000404040000000ull,
		0x0002020200000000ull,
		0x0005050500000000ull,
		0x000a0a0a00000000ull,
		0x0014141400000000ull,
		0x0028282800000000ull,
		0x0050505000000000ull,
		0x00a0a0a000000000ull,
		0x0040404000000000ull,
		0x0202020000000000ull,
		0x0505050000000000ull,
		0x0a0a0a0000000000ull,
		0x1414140000000000ull,
		0x2828280000000000ull,
		0x5050500000000000ull,
		0xa0a0a00000000000ull,
		0x4040400000000000ull,
		0x0202000000000000ull,
		0x0505000000000000ull,
		0x0a0a000000000000ull,
		0x1414000000000000ull,
		0x2828000000000000ull,
		0x5050000000000000ull,
		0xa0a0000000000000ull,
		0x4040000000000000ull
	},
{
		0x0000000000000202ull,
		0x0000000000000505ull,
		0x0000000000000a0aull,
		0x0000000000001414ull,
		0x0000000000002828ull,
		0x0000000000005050ull,
		0x000000000000a0a0ull,
		0x0000000000004040ull,
		0x0000000000020202ull,
		0x0000000000050505ull,
		0x00000000000a0a0aull,
		0x0000000000141414ull,
		0x0000000000282828ull,
		0x0000000000505050ull,
		0x0000000000a0a0a0ull,
		0x0000000000404040ull,
		0x0000000002020200ull,
		0x0000000005050500ull,
		0x000000000a0a0a00ull,
		0x0000000014141400ull,
		0x0000000028282800ull,
		0x0000000050505000ull,
		0x00000000a0a0a000ull,
		0x0000000040404000ull,
		0x0000000202020000ull,
		0x0000000505050000ull,
		0x0000000a0a0a0000ull,
		0x0000001414140000ull,
		0x0000002828280000ull,
		0x0000005050500000ull,
		0x000000a0a0a00000ull,
		0x0000004040400000ull,
		0x0000020202000000ull,
		0x0000050505000000ull,
		0x00000a0a0a000000ull,
		0x0000141414000000ull,
		0x0000282828000000ull,
		0x0000505050000000ull,
		0x0000a0a0a0000000ull,
		0x0000404040000000ull,
		0x0002020200000000ull,
		0x0005050500000000ull,
		0x000a0a0a00000000ull,
		0x0014141400000000ull,
		0x0028282800000000ull,
		0x0050505000000000ull,
		0x00a0a0a000000000ull,
		0x0040404000000000ull,
		0x0202020000000000ull,
		0x0505050000000000ull,
		0x0a0a0a0000000000ull,
		0x1414140000000000ull,
		0x2828280000000000ull,
		0x5050500000000000ull,
		0xa0a0a00000000000ull,
		0x4040400000000000ull,
		0x0202000000000000ull,
		0x0505000000000000ull,
		0x0a0a000000000000ull,
		0x1414000000000000ull,
		0x2828000000000000ull,
		0x5050000000000000ull,
		0xa0a0000000000000ull,
		0x4040000000000000ull
	}
};

void get_pawn_map( position const& p, unsigned long long pawns[] )
{
	pawns[0] = 0;
	pawns[1] = 0;
	for( int c = 0; c < 2; ++c ) {
		for( int i = pieces::pawn1; i <= pieces::pawn8; ++i ) {
			piece const& pp = p.pieces[c][i];
			if( pp.alive && !pp.special ) {
				pawns[c] |= 1ull << (pp.row * 8 + pp.column);
			}
		}
	}
}

short evaluate_pawn( unsigned long long const* pawns, color::type c, unsigned int index )
{
	short ret = 0;
	if( doubled_pawns[c][index] & pawns[c] ) {
		ret += special_values::doubled_pawn;
	}
	else if( !(passed_pawns[c][index] & pawns[1-c]) ) {
		ret += special_values::passed_pawn;
	}
	if( connected_pawns[c][index] & pawns[c] ) {
		ret += special_values::connected_pawn;
	}
	return ret;
}


short evaluate_pawn( unsigned long long const* pawns, color::type c, unsigned int col, unsigned int row )
{
	return evaluate_pawn( pawns, c, row * 8 + col );
}


short evaluate_pawn( position const& p, color::type c, piece const& pp )
{
	unsigned long long pawns[2];
	get_pawn_map( p, pawns );

	return evaluate_pawn( pawns, c, pp.column, pp.row );
}
}

short evaluate_pawns( unsigned long long const* pawns, color::type c )
{
	unsigned short ret = 0;

	unsigned long long p = pawns[c];

	unsigned long long pi;
	while( (pi = __builtin_ffsll(p) ) ) {
		--pi;
		p ^= 1ull << pi;
		ret += evaluate_pawn( pawns, c, pi );
	}

	return ret;
}


short evaluate_side( position const& p, color::type c, unsigned long long* pawns )
{
	short result = 0;

	// To start: Count material in centipawns
	for( unsigned int i = 0; i < 16; ++i) {
		piece const& pp = p.pieces[c][i];
		if( pp.alive ) {
			switch( i ) {
			case pieces::pawn1:
			case pieces::pawn2:
			case pieces::pawn3:
			case pieces::pawn4:
			case pieces::pawn5:
			case pieces::pawn6:
			case pieces::pawn7:
			case pieces::pawn8:
				if (pp.special) {
					// Promoted pawn
					unsigned short promoted = (p.promotions[c] >> ((i - pieces::pawn1) * 2)) & 0x03;
					switch( promoted ) {
					case promotions::queen:
						result += queen_values[c][pp.column][pp.row];
						break;
					case promotions::rook:
						result += rook_values[c][pp.column][pp.row];
						break;
					case promotions::bishop:
						result += bishop_values[c][pp.column][pp.row];
						break;
					case promotions::knight:
						result += knight_values[c][pp.column][pp.row];
						break;
					}
				}
				else {
					result += pawn_values[c][pp.column][pp.row];
					result += evaluate_pawn( pawns, c, pp.column, pp.row );
				}
				break;
			case pieces::king:
				break;
			case pieces::queen:
				result += queen_values[c][pp.column][pp.row];
				break;
			case pieces::rook1:
			case pieces::rook2:
				result += rook_values[c][pp.column][pp.row];
				break;
			case pieces::bishop1:
			case pieces::bishop2:
				result += bishop_values[c][pp.column][pp.row];
				break;
			case pieces::knight1:
			case pieces::knight2:
				result += knight_values[c][pp.column][pp.row];
				break;
			}
		}
	}

	if( p.pieces[c][pieces::bishop1].alive && p.pieces[c][pieces::bishop2].alive ) {
		result += special_values::double_bishop;
	}
	if( p.pieces[c][pieces::king].special ) {
		result += special_values::castled;
	}

	return result;
}

short evaluate( position const& p, color::type c )
{
	unsigned long long pawns[2];
	get_pawn_map( p, pawns );
	int value = evaluate_side( p, c, pawns ) - evaluate_side( p, static_cast<color::type>(1-c), pawns );

	ASSERT( value > result::loss && value < result::win );

	return value;
}

namespace {
static short get_piece_value( position const& p, color::type c, int target, int col, int row )
{
	short eval = 0;
	switch( target ) {
	case pieces::pawn1:
	case pieces::pawn2:
	case pieces::pawn3:
	case pieces::pawn4:
	case pieces::pawn5:
	case pieces::pawn6:
	case pieces::pawn7:
	case pieces::pawn8:
	{
		piece const& pp = p.pieces[c][target];
		if( pp.special ) {
			unsigned short promoted = (p.promotions[c] >> ( 2 * (target - pieces::pawn1) ) ) & 0x03;
			switch( promoted ) {
			case promotions::queen:
				eval += queen_values[c][col][row];
				break;
			case promotions::rook:
				eval += rook_values[c][col][row];
				break;
			case promotions::bishop:
				eval += bishop_values[c][col][row];
				break;
			case promotions::knight:
				eval += knight_values[c][col][row];
				break;
			}
		}
		else {
			eval += pawn_values[c][col][row];
		}
		break;
	}
	case pieces::king:
		break;
	case pieces::queen:
		eval += queen_values[c][col][row];
		break;
	case pieces::rook1:
	case pieces::rook2:
		eval += rook_values[c][col][row];
		break;
	case pieces::bishop1:
	case pieces::bishop2:
		eval += bishop_values[c][col][row];
		break;
	case pieces::knight1:
	case pieces::knight2:
		eval += knight_values[c][col][row];
		break;
	}

	return eval;
}
}


short evaluate_move( position const& p, color::type c, short current_evaluation, move const& m, position::pawn_structure& outPawns )
{
#if 0
	{
		position p2 = p;
		p2.calc_pawn_map();
		if( p.pawns.hash != p2.pawns.hash ) {
			std::cerr << "Pre-eval pawn hash mismatch: " << p.pawns.hash << " " << p2.pawns.hash << " " << move_to_string( p, c, m ) << std::endl;
			exit(1);
		}
		if( evaluate(p2, c) != current_evaluation ) {
			std::cerr << "Pre-eval mismatch: " << current_evaluation << " " << evaluate(p2, c ) << std::endl;
			exit(1);
		}
	}
#endif
	int pawn_move = 0;
	int captured_pawn_index = -1;

	int target = p.board[m.target_col][m.target_row];
	if( target != pieces::nil ) {
		target &= 0x0f;
		current_evaluation += get_piece_value( p, static_cast<color::type>(1-c), target, m.target_col, m.target_row );
		if( target == pieces::bishop1 ) {
			if( p.pieces[1-c][pieces::bishop2].alive ) {
				current_evaluation += special_values::double_bishop;
			}
		}
		else if( target == pieces::bishop2 ) {
			if( p.pieces[1-c][pieces::bishop1].alive ) {
				current_evaluation += special_values::double_bishop;
			}
		}
		else if( target >= pieces::pawn1 && target <= pieces::pawn8 ) {
			piece const& pp = p.pieces[1-c][target];
			if( !pp.special ) {
				captured_pawn_index = m.target_row * 8 + m.target_col;
			}
		}
	}

	int source = p.board[m.source_col][m.source_row] & 0x0f;

	if( source >= pieces::pawn1 && source <= pieces::pawn8 ) {
		piece const& pp = p.pieces[c][source];
		if( pp.special ) {
			current_evaluation -= get_piece_value( p, c, source, pp.column, pp.row );
			current_evaluation += get_piece_value( p, c, source, m.target_col, m.target_row );
		}
		else {
			if( m.target_col != pp.column && target == pieces::nil ) {
				// Was en-passant
				current_evaluation += pawn_values[1-c][m.target_col][pp.row];
				current_evaluation -= pawn_values[c][pp.column][pp.row];
				current_evaluation += pawn_values[c][m.target_col][m.target_row];
				captured_pawn_index = pp.row * 8 + m.target_col;
				pawn_move = 1;
			}
			else if( m.target_row == 0 || m.target_row == 7 ) {
				current_evaluation -= pawn_values[c][pp.column][pp.row];
				current_evaluation += queen_values[c][m.target_col][m.target_row];
				pawn_move = 2;
			}
			else {
				current_evaluation -= pawn_values[c][pp.column][pp.row];
				current_evaluation += pawn_values[c][m.target_col][m.target_row];
				pawn_move = 1;
			}
		}
	}
	else if( source == pieces::king ) {
		piece const& pp = p.pieces[c][source];
		if( pp.column == m.target_col + 2 ) {
			// Queenside
			current_evaluation += special_values::castled;
			current_evaluation -= get_piece_value( p, c, pieces::rook1, 0, pp.row );
			current_evaluation += get_piece_value( p, c, pieces::rook1, 3, m.target_row );
		}
		else if( pp.column + 2 == m.target_col ) {
			// Kingside
			current_evaluation += special_values::castled;
			current_evaluation -= get_piece_value( p, c, pieces::rook2, 7, pp.row );
			current_evaluation += get_piece_value( p, c, pieces::rook2, 5, m.target_row );
		}
	}
	else {
		piece const& pp = p.pieces[c][source];
		current_evaluation -= get_piece_value( p, c, source, pp.column, pp.row );
		current_evaluation += get_piece_value( p, c, source, m.target_col, m.target_row );
	}

	outPawns = p.pawns;
	if( pawn_move || captured_pawn_index != -1 ) {
		if( captured_pawn_index != -1 ) {
			outPawns.map[1-c] &= ~(1ull << captured_pawn_index);
			outPawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(1-c), captured_pawn_index % 8, captured_pawn_index / 8 );
		}
		if( pawn_move ) {
			outPawns.map[c] &= ~(1ull << (m.source_row * 8 + m.source_col) );
			outPawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.source_col, m.source_row );
			if( pawn_move == 1 ) {
				outPawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.target_col, m.target_row );
				outPawns.map[c] |= 1ull << (m.target_row * 8 + m.target_col);
			}
		}

		short pawn_eval;
		if( !pawn_hash_table.lookup( outPawns.hash, pawn_eval ) ) {
			pawn_eval = evaluate_pawns(outPawns.map, color::white) - evaluate_pawns(outPawns.map, color::black);
			pawn_hash_table.store( outPawns.hash, pawn_eval );
		}

		if( c == color::white ) {
			current_evaluation -= outPawns.eval;
			current_evaluation += pawn_eval;
		}
		else {
			current_evaluation += outPawns.eval;
			current_evaluation -= pawn_eval;
		}
		outPawns.eval = pawn_eval;
	}

#if 0
	position p2 = p;
	bool capture;
	apply_move( p2, m, c, capture );
	p2.calc_pawn_map();
	short ev2 = evaluate( p2, c );
	if( outPawns.hash != p2.pawns.hash ) {
		std::cerr << "Pawn hash mismatch: " << p.pawns.hash << " " << p2.pawns.hash << " " << move_to_string( p, c, m ) << std::endl;
		exit(1);
	}
	if( outPawns.map[0] != p2.pawns.map[0] || outPawns.map[1] != p2.pawns.map[1] ) {
		std::cerr << "Pawn map mismatch: " << outPawns.map[0] << " " << outPawns.map[1] << " " << p2.pawns.map[0] << " " << p2.pawns.map[1] << std::endl;
		exit(1);
	}
	if( outPawns.eval != p2.pawns.eval ) {
		std::cerr << "Pawn eval mismatch: " << outPawns.eval << " " << p2.pawns.eval << std::endl;
		exit(1);
	}
	if( ev2 != current_evaluation ) {
		std::cerr << current_evaluation << " " << ev2 << " " << outPawns.eval << " " << p2.pawns.eval << " " << move_to_string( p, c, m ) << " " << captured_pawn_index << " " << pawn_move << std::endl;
		exit(1);
	}
#endif

	return current_evaluation;
}
