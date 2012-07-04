#include <iostream>
#include <iomanip>
#include <cstdlib>

#include "platform.hpp"

int dist( int x1, int y1, int x2, int y2 )
{
	int cx = std::abs(x1 - x2);
	int cy = std::abs(y1 - y2);

	return std::max(cx, cy);
}

int main()
{
	std::cout << "#include \"platform.hpp\"" << std::endl << std::endl;

	/*std::cout << "uint64_t mobility_block[64][64] = {" << std::endl;

	for( unsigned int source = 0; source < 64; ++source ) {
		std::cout << "\t{" << std::endl;

		for( unsigned int blocker = 0; blocker < 64; ++blocker ) {
			std::cout << "\t\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;

			int blocker_col = blocker % 8;
			int blocker_row = blocker / 8;

			int cx = blocker_col - source_col;
			int cy = blocker_row - source_row;

			if( cx || cy ) {
				if( cx == cy || cx == -cy || !cx || !cy) {
					if( cx > 0 ) {
						cx = 1;
					}
					if( cx < 0 ) {
						cx = -1;
					}
					if( cy > 0 ) {
						cy = 1;
					}
					if( cy < 0 ) {
						cy = -1;
					}

					int x = blocker_col + cx;
					int y = blocker_row + cy;
					for( ; x >= 0 && x < 8 && y >= 0 && y < 8; x += cx, y += cy ) {
						v |= 1ull << (x + y * 8);
					}
				}
			}

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( blocker != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}

		if( source != 63 ) {
			std::cout << "\t}," << std::endl;
		}
		else {
			std::cout << "\t}" << std::endl;
		}
	}
	std::cout << "};" << std::endl << std::endl << std::endl;*/

	std::cout << "extern uint64_t const visibility_bishop[64] = {" << std::endl;

	for( unsigned int source = 0; source < 64; ++source ) {
		std::cout << "\t0x";

		uint64_t v = 0;

		int source_col = source % 8;
		int source_row = source / 8;

		for( int cx = -1; cx <= 1; cx += 2 ) {
			for( int cy = -1; cy <= 1; cy += 2 ) {
				int x = source_col + cx;
				int y = source_row + cy;
				for( ; x >= 0 && x < 8 && y >= 0 && y < 8; x += cx, y += cy ) {
					v |= 1ull << (x + y * 8);
				}

			}
		}

		std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

		std::cout << "ull";
		if( source != 63 ) {
			std::cout << ",";
		}
		std::cout << std::endl;
	}
	std::cout << "};" << std::endl << std::endl << std::endl;

	{
		std::cout << "extern uint64_t const visibility_rook[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;

			for( int cx = -1; cx <= 1; ++cx ) {
				for( int cy = -1; cy <= 1; ++cy ) {
					if( !cx == !cy ) {
						continue;
					}
					int x = source_col + cx;
					int y = source_row + cy;
					for( ; x >= 0 && x < 8 && y >= 0 && y < 8; x += cx, y += cy ) {
						v |= 1ull << (x + y * 8);
					}

				}
			}

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl;
	}

	{
		std::cout << "extern uint64_t const pawn_control[2][64] = {" << std::endl;

		for( int c = 0; c < 2; ++c ) {
			std::cout << "\t{" << std::endl;
			for( unsigned int source = 0; source < 64; ++source ) {
				std::cout << "\t\t0x";

				uint64_t v = 0;

				int source_col = source % 8;
				int source_row = source / 8;

				int cy = c ? -1 : 1;
				int y = source_row + cy;

				{
					int x = source_col + 1;
					if( x >= 0 && x < 8 && y >= 0 && y < 8 ) {
						v |= 1ull << (x + y * 8);
					}
				}
				{
					int x = source_col - 1;
					if( x >= 0 && x < 8 && y >= 0 && y < 8 ) {
						v |= 1ull << (x + y * 8);
					}
				}


				std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

				std::cout << "ull";
				if( source != 63 ) {
					std::cout << ",";
				}
				std::cout << std::endl;
			}
			if( c ) {
				std::cout << "\t}" << std::endl;
			}
			else {
				std::cout << "\t}," << std::endl;
			}
		}
		std::cout << "};" << std::endl << std::endl;
	}

	{
		std::cout << "extern short const proximity[64][64] = {" << std::endl;

		for( int target = 0; target < 64; ++target ) {
			std::cout << "\t{" << std::endl;
			for( unsigned int source = 0; source < 64; ++source ) {
				std::cout << "\t\t";

				short v = 0;

				int source_col = source % 8;
				int source_row = source / 8;
				int target_col = target % 8;
				int target_row = target / 8;

				v = 7 - std::max( std::abs(source_col - target_col), std::abs(source_row - target_row) );

				std::cout << std::dec << v;

				if( source != 63 ) {
					std::cout << ",";
				}
				std::cout << std::endl;
			}
			if( target == 63 ) {
				std::cout << "\t}" << std::endl;
			}
			else {
				std::cout << "\t}," << std::endl;
			}
		}
		std::cout << "};" << std::endl;
	}

	{
		std::cout << "extern short const king_distance[64][64] = {" << std::endl;

		for( int target = 0; target < 64; ++target ) {
			std::cout << "\t{" << std::endl;
			for( unsigned int source = 0; source < 64; ++source ) {
				std::cout << "\t\t";

				short v = 0;

				int source_col = source % 8;
				int source_row = source / 8;
				int target_col = target % 8;
				int target_row = target / 8;

				v = std::max( std::abs(source_col - target_col), std::abs(source_row - target_row) );

				std::cout << std::dec << v;

				if( source != 63 ) {
					std::cout << ",";
				}
				std::cout << std::endl;
			}
			if( target == 63 ) {
				std::cout << "\t}" << std::endl;
			}
			else {
				std::cout << "\t}," << std::endl;
			}
		}
		std::cout << "};" << std::endl;
	}

	{
		std::cout << "extern uint64_t const ray_n[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;


			for( int y = source_row + 1; y < 8; ++y ) {
				v |= 1ull << (source_col + y * 8);
			}

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}

	{
		std::cout << "extern uint64_t const ray_e[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;


			for( int x = source_col + 1; x < 8; ++x ) {
				v |= 1ull << (x + source_row * 8);
			}

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}

	{
		std::cout << "extern uint64_t const ray_s[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;


			for( int y = source_row - 1; y >= 0; --y ) {
				v |= 1ull << (source_col + y * 8);
			}

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}

	{
		std::cout << "extern uint64_t const ray_w[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;


			for( int x = source_col - 1; x >= 0; --x ) {
				v |= 1ull << (x + source_row * 8);
			}

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}

	{
		std::cout << "extern uint64_t const ray_ne[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;

			int x = source_col + 1;
			int y = source_row + 1;
			for( ; x < 8 && y < 8; ++x, ++y ) {
				v |= 1ull << (x + y * 8);
			}

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}

	{
		std::cout << "extern uint64_t const ray_se[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;

			int x = source_col + 1;
			int y = source_row - 1;
			for( ; x < 8 && y >= 0; ++x, --y ) {
				v |= 1ull << (x + y * 8);
			}

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}

	{
		std::cout << "extern uint64_t const ray_sw[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;

			int x = source_col - 1;
			int y = source_row - 1;
			for( ; x >= 0 && y >= 0; --x, --y ) {
				v |= 1ull << (x + y * 8);
			}

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}

	{
		std::cout << "extern uint64_t const ray_nw[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;

			int x = source_col - 1;
			int y = source_row + 1;
			for( ; x >= 0 && y < 8; --x, ++y ) {
				v |= 1ull << (x + y * 8);
			}

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}

	{
		std::cout << "extern uint64_t const isolated_pawns[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;

			for( int y = 0; y < 8; ++y ) {
				if( source_col > 0 ) {
					v |= 1ull << (source_col - 1 + y * 8);
				}
				if( source_col < 7 ) {
					v |= 1ull << (source_col + 1 + y * 8);
				}
			}

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}

	uint64_t possible_king_moves[64];

	{
		std::cout << "extern uint64_t const possible_king_moves[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;

			for( int cx = -1; cx <= 1; ++cx ) {
				for( int cy = -1; cy <= 1; ++cy ) {
					if( !cx && !cy ) {
						continue;
					}

					int x = source_col + cx;
					int y = source_row + cy;
					if( x >= 0 && x <= 7 && y >= 0 && y <= 7 ) {
						v |= 1ull << (x + y * 8);
					}
				}
			}

			possible_king_moves[source] = v;

			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}

	{
		std::cout << "extern uint64_t const possible_knight_moves[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			uint64_t v = 0;

			int source_col = source % 8;
			int source_row = source / 8;

			for( int cx = -2; cx <= 2; ++cx ) {
				for( int cy = -2; cy <= 2; ++cy ) {
					if( !cx || !cy || cx == cy || cx == -cy ) {
						continue;
					}

					int x = source_col + cx;
					int y = source_row + cy;
					if( x >= 0 && x <= 7 && y >= 0 && y <= 7 ) {
						v |= 1ull << (x + y * 8);
					}
				}
			}
			
			std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

			std::cout << "ull";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}

	{
		uint64_t rule_of_the_square[2][2][64];

		for( int c = 0; c < 2; ++c ) {
			for( int king = 0; king < 64; ++king ) {

				uint64_t v = 0;

				int king_x = king % 8;
				int king_y = king / 8;

				int king_cy = c ? king_y : (7 - king_y);

				for( int pawn = 0; pawn < 64; ++pawn ) {
					int pawn_x = pawn % 8;
					int pawn_y = pawn / 8;

					// To acccount for pawn double move
					if( pawn_y == (c ? 6 : 1) ) {
						pawn_y = (c ? 5 : 2);
					}
					int pawn_cy = c ? pawn_y : (7 - pawn_y);

					int d = dist( king_x, king_y, pawn_x, c ? 0 : 7 );
					if( d <= pawn_cy ) {
						v |= 1ull << pawn;
					}
				}

				rule_of_the_square[1-c][c][king] = v;
			}
		}
		for( int c = 0; c < 2; ++c ) {
			for( int king = 0; king < 64; ++king ) {
				rule_of_the_square[c][c][king] = possible_king_moves[king];
				uint64_t kings = possible_king_moves[king];
				while( kings ) {
					uint64_t kk = bitscan_unset( kings );
					rule_of_the_square[c][c][king] |= rule_of_the_square[c][1-c][kk];
				}
			}
		}

		std::cout << "extern uint64_t const rule_of_the_square[2][2][64] = { " << std::endl;
		for( int c = 0; c < 2; ++c ) {
			std::cout << "\t{" << std::endl;
			for( int stm = 0; stm < 2; ++stm ) {
				std::cout << "\t\t{" << std::endl;
				for( int king = 0; king < 64; ++king ) {
					std::cout << "\t\t\t0x";
					std::cout << std::hex << std::setw(16) << std::setfill('0') << rule_of_the_square[c][stm][king];
					std::cout << "ull";
					if( king != 63 ) {
						std::cout << ",";
					}
					std::cout << std::endl;
				}
				std::cout << "\t\t}";
				if( !stm ) {
					std::cout << ",";
				}
				std::cout << std::endl;
			}
			std::cout << "\t}";
			if( !c ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}

		std::cout << "};" << std::endl;
	}


	{
		std::cout << "extern uint64_t const king_attack_zone[2][64] = {" << std::endl;

		for( int c = 0; c < 2; ++c ) {
			std::cout << "\t{" << std::endl;
			for( unsigned int source = 0; source < 64; ++source ) {
				std::cout << "\t\t0x";

				uint64_t v = possible_king_moves[source];
				if( c ) {
					v >>= 8;
				}
				else {
					v <<= 8;
				}
				v |= possible_king_moves[source];

				std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

				std::cout << "ull";
				if( source != 63 ) {
					std::cout << ",";
				}
				std::cout << std::endl;
			}
			std::cout << "\t}";
			if( !c ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}


	{
		std::cout << "extern uint64_t const between_squares[64][64] = {" << std::endl;

		for( int source = 0; source < 64; ++source ) {
			std::cout << "\t{" << std::endl;
			for( int target = 0; target < 64; ++target ) {
				std::cout << "\t\t0x";

				uint64_t v = 0;

				int cx = target % 8 - source % 8;
				int cy = target / 8 - source / 8;

				if( cx || cy ) {
					if( cx == cy || cx == -cy || !cx || !cy ) {
						if( cx > 0 ) {
							cx = 1;
						}
						else if( cx < 0 ) {
							cx = -1;
						}
						if( cy > 0 ) {
							cy = 1;
						}
						else if( cy < 0 ) {
							cy = -1;
						}

						for( int sq = source + cx + cy * 8; sq != target; sq += cx + cy * 8 ) {
							v |= 1ull << sq;
						}
					}
				}

				std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

				std::cout << "ull";
				if( target != 63 ) {
					std::cout << ",";
				}
				std::cout << std::endl;
			}
			std::cout << "\t}";
			if( source != 63 ) {
				std::cout << ",";
			}
			std::cout << std::endl;
		}
		std::cout << "};" << std::endl << std::endl << std::endl;
	}


	{
		std::cout << "extern uint64_t const connected_pawns[2][64] = {" << std::endl;

		for( int c = 0; c < 2; ++c ) {
			std::cout << "\t{" << std::endl;
			for( unsigned int source = 0; source < 64; ++source ) {
				std::cout << "\t\t0x";

				uint64_t v = 0;

				int source_col = source % 8;
				int source_row = source / 8;

				int y = source_row;
				for( int i = 0; i < 2; ++i ) {
					if( y != 0 && y != 7 ) {
						{
							int x = source_col + 1;
							if( x >= 0 && x < 8 ) {
								v |= 1ull << (x + y * 8);
							}
						}
						{
							int x = source_col - 1;
							if( x >= 0 && x < 8 ) {
								v |= 1ull << (x + y * 8);
							}
						}
						y += c ? 1 : -1;
					}
				}

				std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

				std::cout << "ull";
				if( source != 63 ) {
					std::cout << ",";
				}
				std::cout << std::endl;
			}
			if( c ) {
				std::cout << "\t}" << std::endl;
			}
			else {
				std::cout << "\t}," << std::endl;
			}
		}
		std::cout << "};" << std::endl << std::endl;
	}


	{
		std::cout << "extern uint64_t const passed_pawns[2][64] = {" << std::endl;

		for( int c = 0; c < 2; ++c ) {
			std::cout << "\t{" << std::endl;
			for( unsigned int source = 0; source < 64; ++source ) {
				std::cout << "\t\t0x";

				uint64_t v = 0;

				int source_col = source % 8;
				int source_row = source / 8;

				int cy = c ? -1 : 1;

				for( int y = source_row + cy; y >= 0 && y <= 7; y += cy ) {
					for( int cx = -1; cx <= 1; ++cx ) {
						int x = source_col + cx;
						if( x >= 0 && x < 8 ) {
							v |= 1ull << (x + y * 8);
						}
					}
				}

				std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

				std::cout << "ull";
				if( source != 63 ) {
					std::cout << ",";
				}
				std::cout << std::endl;
			}
			if( c ) {
				std::cout << "\t}" << std::endl;
			}
			else {
				std::cout << "\t}," << std::endl;
			}
		}
		std::cout << "};" << std::endl << std::endl;
	}


	{
		std::cout << "extern uint64_t const doubled_pawns[2][64] = {" << std::endl;

		for( int c = 0; c < 2; ++c ) {
			std::cout << "\t{" << std::endl;
			for( unsigned int source = 0; source < 64; ++source ) {
				std::cout << "\t\t0x";

				uint64_t v = 0;

				int source_col = source % 8;
				int source_row = source / 8;

				if( source_row >= 0 && source_row <= 7 ) {

					int cy = c ? -1 : 1;

					for( int y = source_row + cy; y >= 0 && y < 8; y += cy ) {
						v |= 1ull << (source_col + y * 8);
					}
				}

				std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

				std::cout << "ull";
				if( source != 63 ) {
					std::cout << ",";
				}
				std::cout << std::endl;
			}
			if( c ) {
				std::cout << "\t}" << std::endl;
			}
			else {
				std::cout << "\t}," << std::endl;
			}
		}
		std::cout << "};" << std::endl << std::endl;
	}


	{
		std::cout << "extern uint64_t const forward_pawn_attack[2][64] = {" << std::endl;

		for( int c = 0; c < 2; ++c ) {
			std::cout << "\t{" << std::endl;
			for( unsigned int source = 0; source < 64; ++source ) {
				std::cout << "\t\t0x";

				uint64_t v = 0;

				int source_col = source % 8;
				int source_row = source / 8;

				int cy = c ? -1 : 1;

				for( int y = source_row + cy; y > 0 && y < 7; y += cy ) {
					for( int cx = -1; cx <= 1; cx += 2 ) {
						int x = source_col + cx;
						if( x >= 0 && x < 8 ) {
							v |= 1ull << (x + y * 8);
						}
					}
				}

				std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

				std::cout << "ull";
				if( source != 63 ) {
					std::cout << ",";
				}
				std::cout << std::endl;
			}
			if( c ) {
				std::cout << "\t}" << std::endl;
			}
			else {
				std::cout << "\t}," << std::endl;
			}
		}
		std::cout << "};" << std::endl << std::endl;
	}

	{
		std::cout << "extern uint64_t const trapped_bishop[2][64] = {" << std::endl;

		for( int c = 0; c < 2; ++c ) {
			std::cout << "\t{" << std::endl;
			for( unsigned int square = 0; square < 64; ++square ) {
				std::cout << "\t\t0x";

				uint64_t v = 0;

				int col = square % 8;
				int row = square / 8;

				if( (c && (square == 8 || square == 16 || square == 1 || square == 15 || square == 23 || square == 6 )) ||
					(!c && (square == 55 || square == 47  || square == 62 || square == 48 || square == 40 || square == 57 )) ) {

					int cx = (col > 4) ? -1 : 1;
					int cy = c ? 1 : -1;

					v |= 1ull << (col + cx + (row + cy) * 8);
				}

				std::cout << std::hex << std::setw(16) << std::setfill('0') << v;

				std::cout << "ull";
				if( square != 63 ) {
					std::cout << ",";
				}
				std::cout << std::endl;
			}
			if( c ) {
				std::cout << "\t}" << std::endl;
			}
			else {
				std::cout << "\t}," << std::endl;
			}
		}
		std::cout << "};" << std::endl << std::endl;
	}

	return 0;
}
