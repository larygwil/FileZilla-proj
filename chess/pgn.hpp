#ifndef __PGN_H__
#define __PGN_H__

#include "chess.hpp"

#include <list>
#include <fstream>

struct game
{
	position p_;
	color::type c_;
	std::list<move> moves_;
};

class pgn_reader
{
public:
	pgn_reader();

	bool open( std::string const& file );

	bool next( game& g );

private:
	bool next_line( std::string& line );

	std::ifstream in_;
	std::string pushback_;
};

#endif
