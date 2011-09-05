#include <iostream>
#include <iomanip>

int main()
{
	std::cout << "namespace {" << std::endl;
	std::cout << "static unsigned long long mobility_block[64][64] = {" << std::endl;

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
	std::cout << "};" << std::endl << std::endl << std::endl;

	std::cout << "static unsigned long long visibility_bishop[64] = {" << std::endl;

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

	std::cout << "static unsigned long long visibility_rook[64] = {" << std::endl;

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
	std::cout << "};" << std::endl;

	std::cout << "}" << std::endl;

	return 0;
}
