#include "history.hpp"

history::history()
{
	clear();
}


void history::clear()
{
	for( int c = 0; c < 2; ++c ) {
		for( int pi = 0; pi < 7; ++pi ) {
			for( int sq = 0; sq < 64; ++sq ) {
				all_[c][pi][sq] = 1;
				cut_[c][pi][sq] = 1;
			}
		}
	}
};


void history::reduce()
{
	for( int c = 0; c < 2; ++c ) {
		for( int pi = 0; pi < 7; ++pi ) {
			for( int sq = 0; sq < 64; ++sq ) {
				all_[c][pi][sq] /= 2;
				cut_[c][pi][sq] /= 2;
				++all_[c][pi][sq];
				++cut_[c][pi][sq];
			}
		}
	}
}

int history::get_value( move const& m, color::type c ) const
{
	return static_cast<int>((cut_[c][m.piece][m.target] << 24ull) / all_[c][m.piece][m.target]);
}

void history::record_cut(const move_info *begin, const move_info *end, color::type c )
{
	move_info const* cut = end - 1;
	++cut_[c][cut->m.piece][cut->m.target];
	for( move_info const* it = begin; it != end; ++it ) {
		++all_[c][it->m.piece][it->m.target];
	}
}