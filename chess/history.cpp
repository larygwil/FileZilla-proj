#include "history.hpp"

history::history()
{
	clear();
}


void history::clear()
{
	for( int c = 0; c < 2; ++c ) {
		for( int pi = pieces::pawn; pi <= pieces::king; ++pi ) {
			for( int sq = 0; sq < 64; ++sq ) {
				all_[c][pi][sq] = 1;
				cut_[c][pi][sq] = 1;
			}
		}
	}
}


void history::reduce()
{
	// TODO: Dividing by two, is it strong enough? Perhaps just a clear.
	for( int c = 0; c < 2; ++c ) {
		for( int pi = pieces::pawn; pi <= pieces::king; ++pi ) {
			for( int sq = 0; sq < 64; ++sq ) {
				all_[c][pi][sq] /= 2;
				cut_[c][pi][sq] /= 2;
				++all_[c][pi][sq];
				++cut_[c][pi][sq];
			}
		}
	}
}

int history::get_value( pieces::type piece, move const& m, color::type c ) const
{
	return static_cast<int>((cut_[c][piece][m.target()] << 24ull) / all_[c][piece][m.target()]);
}

void history::record( position const& p, move const& m )
{
	if( p.get_captured_piece( m ) != pieces::none ) {
		return;
	}

	++all_[p.self()][p.get_piece(m.source())][m.target()];
}

void history::record_cut( position const& p, move const& m, int processed )
{
	ASSERT( p.get_captured_piece( m ) == pieces::none );

	cut_[p.self()][p.get_piece(m.source())][m.target()] += 1 + processed / 2;
}