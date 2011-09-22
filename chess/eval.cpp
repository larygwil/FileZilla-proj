#include "eval.hpp"
#include "assert.hpp"
#include "mobility.hpp"
#include "util.hpp"
#include "pawn_structure_hash_table.hpp"
#include "zobrist.hpp"

#include <iostream>

namespace special_values {
enum type
{
	double_bishop = 25,
	doubled_pawn = -50,
	passed_pawn = 35,
	connected_pawn = 15,
	pawn_shield = 3,
	castled = 25
};
}

namespace {
unsigned char const pawn_values[2][8][8] =
	{
		{
			{ 0, 100, 102, 103, 104, 106, 145, 0 },
			{ 0, 100, 102, 105, 108, 112, 150, 0 },
			{ 0, 100, 102, 110, 117, 125, 155, 0 },
			{ 0,  80, 105, 120, 127, 140, 160, 0 },
			{ 0,  80, 105, 120, 127, 140, 160, 0 },
			{ 0, 100, 102, 110, 117, 125, 155, 0 },
			{ 0, 100, 102, 105, 108, 112, 150, 0 },
			{ 0, 100, 102, 103, 104, 106, 145, 0 }
		},
		{
			{ 0, 145, 106, 104, 103, 102, 100, 0 },
			{ 0, 150, 112, 108, 105, 102, 100, 0 },
			{ 0, 155, 125, 117, 110, 102, 100, 0 },
			{ 0, 160, 140, 127, 120, 105,  80, 0 },
			{ 0, 160, 140, 127, 120, 105,  80, 0 },
			{ 0, 155, 125, 117, 110, 102, 100, 0 },
			{ 0, 150, 112, 108, 105, 102, 100, 0 },
			{ 0, 145, 106, 104, 103, 102, 100, 0 }
		}

	};


signed short const q = material_values::queen;
signed short const queen_values[2][8][8] = {
	{
		{ q-20, q-10, q-10, q-5 , q   , q-10, q-10, q-20 },
		{ q-10, q   , q+5 , q   , q   , q   , q   , q-10 },
		{ q-10, q+5 , q+5 , q+5 , q+5 , q+5 , q   , q-10 },
		{ q-5 , q   , q+5 , q+5 , q+5 , q+5 , q   , q-5  },
		{ q-5 , q   , q+5 , q+5 , q+5 , q+5 , q   , q-5  },
		{ q-10, q   , q+5 , q+5 , q+5 , q+5 , q   , q-10 },
		{ q-10, q   , q   , q   , q   , q   , q   , q-10 },
		{ q-20, q-10, q-10, q-5 , q-5 , q-10, q-10, q-20 }
	},
	{
		{ q-20, q-10, q-10, q-5 , q   , q-10, q-10, q-20 },
		{ q-10, q   , q   , q   , q   , q   , q   , q-10 },
		{ q-10, q   , q+5 , q+5 , q+5 , q+5 , q   , q-10 },
		{ q-5 , q   , q+5 , q+5 , q+5 , q+5 , q   , q-5  },
		{ q-5 , q   , q+5 , q+5 , q+5 , q+5 , q   , q-5  },
		{ q-10, q   , q+5 , q+5 , q+5 , q+5 , q+5 , q-10 },
		{ q-10, q   , q   , q   , q   , q+5 , q   , q-10 },
		{ q-20, q-10, q-10, q-5 , q-5 , q-10, q-10, q-20 }
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

signed short const b = material_values::bishop;
signed short const bishop_values[2][8][8] = {
	{
		{ b-10, b   , b+3 , b+5 , b+ 5, b+3 , b   , b   },
		{ b-9 , b+10, b+6 , b+10, b+10, b+6 , b+10, b   },
		{ b-7 , b+5 , b+15, b+12, b+12, b+15, b+5 , b+2 },
		{ b-5 , b+10, b+12, b+20, b+20, b+12, b+10, b+5 },
		{ b-5 , b+10, b+12, b+20, b+20, b+12, b+10, b+5 },
		{ b-7 , b+5 , b+15, b+12, b+12, b+15, b+5 , b+2 },
		{ b-9 , b+10, b+6 , b+10, b+10, b+6 , b+10, b   },
		{ b-10, b   , b+3 , b+5 , b+ 5, b+3 , b   , b   }
	},
	{
		{ b  , b   , b+3 , b+5 , b+ 5, b+3 , b   , b-10 },
		{ b  , b+10, b+6 , b+10, b+10, b+6 , b+10, b-9  },
		{ b+2, b+5 , b+15, b+12, b+12, b+15, b+5 , b-7  },
		{ b+5, b+10, b+12, b+20, b+20, b+12, b+10, b-5  },
		{ b+5, b+10, b+12, b+20, b+20, b+12, b+10, b-5  },
		{ b+2, b+5 , b+15, b+12, b+12, b+15, b+5 , b-7  },
		{ b  , b+10, b+6 , b+10, b+10, b+6 , b+10, b-9  },
		{ b  , b   , b+3 , b+5 , b+ 5, b+3 , b   , b-10 }
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


short evaluate_pawn( unsigned long long own_pawns, unsigned long long foreign_pawns, color::type c, unsigned long long pawn )
{
	short ret = 0;
	if( doubled_pawns[c][pawn] & own_pawns ) {
		ret += special_values::doubled_pawn;
	}
	else if( !(passed_pawns[c][pawn] & foreign_pawns) ) {
		ret += special_values::passed_pawn;
	}
	if( connected_pawns[c][pawn] & own_pawns ) {
		ret += special_values::connected_pawn;
	}
	return ret;
}
}

short evaluate_pawns( unsigned long long white_pawns, unsigned long long black_pawns )
{
	short ret = 0;

	{
		unsigned long long pawns = white_pawns;
		while( pawns ) {
			unsigned long long pawn;
			bitscan( pawns, pawn );
			pawns ^= 1ull << pawn;

			ret += evaluate_pawn( white_pawns, black_pawns, color::white, pawn );
		}
	}


	{
		unsigned long long pawns = black_pawns;
		while( pawns ) {
			unsigned long long pawn;
			bitscan( pawns, pawn );
			pawns ^= 1ull << pawn;

			ret -= evaluate_pawn( black_pawns, white_pawns, color::black, pawn );
		}
	}

	return ret;
}


short evaluate_side( position const& p, color::type c )
{
	short result = 0;

	unsigned long long pieces = p.bitboards[c].all_pieces;
	while( pieces ) {
		unsigned long long piece;
		bitscan( pieces, piece );

		unsigned long long bpiece = 1ull << piece;
		pieces ^= bpiece;

		unsigned long long col = piece % 8;
		unsigned long long row = piece / 8;

		if( p.bitboards[c].pawns & bpiece ) {
			result += pawn_values[c][col][row];
		}
		else if( p.bitboards[c].knights & bpiece ) {
			result += knight_values[c][col][row];
		}
		else if( p.bitboards[c].bishops & bpiece ) {
			result += bishop_values[c][col][row];
		}
		else if( p.bitboards[c].rooks & bpiece ) {
			result += rook_values[c][col][row];
		}
		else if( p.bitboards[c].queens & bpiece ) {
			result += queen_values[c][col][row];
		}
	}

	if( popcount( p.bitboards[c].bishops ) >= 2 ) {
		result += special_values::double_bishop;
	}

	if( p.castle[c] & 0x4 ) {
		result += special_values::castled;
	}

	return result;
}

short evaluate_fast( position const& p, color::type c )
{
	int value = evaluate_side( p, c ) - evaluate_side( p, static_cast<color::type>(1-c) );

	if( c ) {
		value -= evaluate_pawns( p.bitboards[0].pawns, p.bitboards[1].pawns );
	}
	else {
		value += evaluate_pawns( p.bitboards[0].pawns, p.bitboards[1].pawns );
	}

	ASSERT( value > result::loss && value < result::win );

	return value;
}

namespace {
static short get_piece_value( color::type c, pieces2::type target, int col, int row )
{
	switch( target ) {
	case pieces2::pawn:
		return pawn_values[c][col][row];
	case pieces2::knight:
		return knight_values[c][col][row];
	case pieces2::bishop:
		return bishop_values[c][col][row];
	case pieces2::rook:
		return rook_values[c][col][row];
	case pieces2::queen:
		return queen_values[c][col][row];
	default:
	case pieces2::king:
		return 0;
	}
}
}


short evaluate_move( position const& p, color::type c, short current_evaluation, move const& m, position::pawn_structure& outPawns )
{
#if 0
	short old_eval = current_evaluation;
	{
		short fresh_eval = evaluate_fast( p, c );
		if( fresh_eval != current_evaluation ) {
			std::cerr << "BAD" << std::endl;
		}
	}
#endif

	if( m.flags & move_flags::castle ) {
		
		current_evaluation += special_values::castled;

		if( m.target_col == 6 ) {
			// Kingside
			current_evaluation -= rook_values[c][7][m.source_row];
			current_evaluation += rook_values[c][5][m.source_row];
		}
		else {
			// Queenside
			current_evaluation -= rook_values[c][0][m.source_row];
			current_evaluation += rook_values[c][3][m.source_row];
		}

		outPawns = p.pawns;

		return current_evaluation;
	}

	if( m.captured_piece != pieces2::none ) {
		if( m.flags & move_flags::enpassant ) {
			current_evaluation += pawn_values[1-c][m.target_col][m.source_row];
		}
		else {
			current_evaluation += get_piece_value( static_cast<color::type>(1-c), m.captured_piece, m.target_col, m.target_row );
		}

		if( m.captured_piece == pieces2::bishop && popcount( p.bitboards[1-c].bishops ) == 2 ) {
			current_evaluation += special_values::double_bishop;
		}
	}

	if( m.flags & move_flags::promotion ) {
		current_evaluation -= pawn_values[c][m.source_col][m.source_row];
		current_evaluation += queen_values[c][m.target_col][m.target_row];
	}
	else {
		current_evaluation -= get_piece_value( c, m.piece, m.source_col, m.source_row );
		current_evaluation += get_piece_value( c, m.piece, m.target_col, m.target_row );
	}

	outPawns = p.pawns;
	if( m.piece == pieces2::pawn || m.captured_piece == pieces2::pawn ) {
		unsigned long long pawnMap[2];
		pawnMap[0] = p.bitboards[0].pawns;
		pawnMap[1] = p.bitboards[1].pawns;

		if( m.captured_piece == pieces2::pawn ) {
			if( m.flags & move_flags::enpassant ) {
				pawnMap[1-c] &= ~(1ull << (m.target_col + m.source_row * 8) );
				outPawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(1-c), m.target_col, m.source_row );
			}
			else {
				pawnMap[1-c] &= ~(1ull << (m.target_col + m.target_row * 8) );
				outPawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(1-c), m.target_col, m.target_row );
			}
		}
		if( m.piece == pieces2::pawn ) {
			pawnMap[c] &= ~(1ull << (m.source_row * 8 + m.source_col) );
			outPawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.source_col, m.source_row );
			if( !(m.flags & move_flags::promotion) ) {
				outPawns.hash ^= get_pawn_structure_hash( static_cast<color::type>(c), m.target_col, m.target_row );
				pawnMap[c] |= 1ull << (m.target_row * 8 + m.target_col);
			}
		}

		short pawn_eval;
		if( !pawn_hash_table.lookup( outPawns.hash, pawn_eval ) ) {
			pawn_eval = evaluate_pawns(pawnMap[0], pawnMap[1]);
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
	{
		position p2 = p;
		p2.init_pawn_structure();

		position p4 = p;
		p4.init_pawn_structure();

		bool captured;
		apply_move( p2, m, c, captured );
		short fresh_eval = evaluate_fast( p2, c );
		if( fresh_eval != current_evaluation ) {
			std::cerr << "BAD" << std::endl;

			position p3 = p;
			bool captured;
			apply_move( p3, m, c, captured );

			evaluate_move( p, c, evaluate_fast( p, c ), m, outPawns );
		}
	}
#endif

	return current_evaluation;
}


unsigned char const shield_const = 5;


/* Pawn shield for king
 * 1 2 3
 * 4 5 6
 *   K
 * Pawns on 1, 3, 4, 6 are awarded shield_const points, pawns on 2 and 5 are awarded twice as many points.
 * Double pawns do not count.
 *
 * Special case: a and h file. There b and g also count twice.
 */
short evaluate_pawn_shield_side( position const& p, color::type c )
{
	short ev = 0;

	int const y = c ? 40 : 8;

	unsigned long long kings = p.bitboards[c].king;
	unsigned long long king;
	bitscan( kings, king );

	unsigned char king_col = static_cast<unsigned char>( king % 8 );
	unsigned char king_row = static_cast<unsigned char>( king / 8 );

	if( king_row == (c ? 7 : 0) ) {
		if( p.bitboards[c].pawns & (9ull << (king_col + y) ) ) {
			ev += special_values::pawn_shield * 2;
		}
		if( king_col ) {
			if( p.bitboards[c].pawns & (9ull << (king_col - 1 + y) ) ) {
				if( king_col == 7 ) {
					ev += special_values::pawn_shield * 2;
				}
				else {
					ev += special_values::pawn_shield;
				}
			}
		}
		if( king_col != 7 ) {
			if( p.bitboards[c].pawns & (9ull << (king_col + 1 + y) ) ) {
				if( king_col == 0 ) {
					ev += special_values::pawn_shield * 2;
				}
				else {
					ev += special_values::pawn_shield;
				}
			}
		}
	}

	return ev;
}


short evaluate_pawn_shield( position const& p, color::type c )
{

	short own = evaluate_pawn_shield_side( p, c );
	short other = evaluate_pawn_shield_side( p, static_cast<color::type>(1-c) );

	short ev = own - other;

	return ev;
}

short evaluate_full( position const& p, color::type c )
{
	short eval = evaluate_fast( p, c );

	return evaluate_full( p, c, eval );
}

extern unsigned long long pawn_control[2][64];

short evaluate_full( position const& p, color::type c, short eval_fast )
{
	eval_fast += evaluate_pawn_shield( p, c );

	for( int i = 0; i < 2; ++i ) {
		p.bitboards[i].pawn_control = 0;
		unsigned long long pawn;
		unsigned long long pawns = p.bitboards[i].pawns;
		while( pawns ) {
			bitscan( pawns, pawn );
			pawns ^= 1ull << pawn;
			p.bitboards[i].pawn_control |= pawn_control[i][pawn];
		}
	}

	eval_fast += evaluate_mobility( p, c, p.bitboards );
	
	// Adjust score based on material. The basic idea is that,
	// given two positions with equal, non-zero score,
	// the position having fewer material is better.
	short v = 25 * (std::max)( 0, material_values::initial * 2 - p.material[0] - p.material[1] ) / (material_values::initial * 2);
	if( eval_fast > 0 ) {
		eval_fast += v;
	}
	else if( eval_fast < 0 ) {
		eval_fast -= v;
	}

	return eval_fast;
}

short get_material_value( pieces2::type pi )
{
	return static_cast<material_values::type>(pi);
}
