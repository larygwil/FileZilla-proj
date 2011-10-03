#include <iostream>
#include <iomanip>

#include <cmath>

int main()
{
	/*std::cout << "unsigned long long mobility_block[64][64] = {" << std::endl;

	for( unsigned int source = 0; source < 64; ++source ) {
		std::cout << "\t{" << std::endl;

		for( unsigned int blocker = 0; blocker < 64; ++blocker ) {
			std::cout << "\t\t0x";

			unsigned long long v = 0;

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

	std::cout << "unsigned long long visibility_bishop[64] = {" << std::endl;

	for( unsigned int source = 0; source < 64; ++source ) {
		std::cout << "\t0x";

		unsigned long long v = 0;

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
		std::cout << "unsigned long long visibility_rook[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			unsigned long long v = 0;

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
		std::cout << "extern unsigned long long const pawn_control[2][64] = {" << std::endl;

		for( int c = 0; c < 2; ++c ) {
			std::cout << "\t{" << std::endl;
			for( unsigned int source = 0; source < 64; ++source ) {
				std::cout << "\t\t0x";

				unsigned long long v = 0;

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
		std::cout << "short proximity[64][64] = {" << std::endl;

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
		std::cout << "unsigned long long ray_n[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			unsigned long long v = 0;

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
		std::cout << "unsigned long long ray_e[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			unsigned long long v = 0;

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
		std::cout << "unsigned long long ray_s[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			unsigned long long v = 0;

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
		std::cout << "unsigned long long ray_w[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			unsigned long long v = 0;

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
		std::cout << "unsigned long long ray_ne[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			unsigned long long v = 0;

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
		std::cout << "unsigned long long ray_se[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			unsigned long long v = 0;

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
		std::cout << "unsigned long long ray_sw[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			unsigned long long v = 0;

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
		std::cout << "unsigned long long ray_nw[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			unsigned long long v = 0;

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
		std::cout << "extern unsigned long long const king_pawn_shield[2][64] = {" << std::endl;

		for( int c = 0; c < 2; ++c ) {
			std::cout << "\t{" << std::endl;
			for( unsigned int source = 0; source < 64; ++source ) {
				std::cout << "\t\t0x";

				unsigned long long v = 0;

				int source_col = source % 8;
				int source_row = source / 8;

				if( ((!c && source_row < 2) || (c && source_row >= 6 )) &&
					(source_col < 3 || source_col > 4) ) {

					int cy = c ? -1 : 1;
					int y = c ? 6 : 1;

					{
						int x = source_col + 1;
						if( x >= 0 && x < 8 && y >= 0 && y < 8 ) {
							v |= 1ull << (x + y * 8);
							v |= 1ull << (x + (y + cy) * 8);
						}
					}
					v |= 1ull << (source_col + y * 8);
					v |= 1ull << (source_col + (y + cy) * 8);
					{
						int x = source_col - 1;
						if( x >= 0 && x < 8 && y >= 0 && y < 8 ) {
							v |= 1ull << (x + (y + cy) * 8);
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
		std::cout << "extern unsigned long long const isolated_pawns[64] = {" << std::endl;

		for( unsigned int source = 0; source < 64; ++source ) {
			std::cout << "\t0x";

			unsigned long long v = 0;

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

	return 0;
}
