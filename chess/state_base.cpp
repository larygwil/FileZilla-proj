#include "state_base.hpp"

#include "assert.hpp"
#include "util.hpp"

state_base::state_base( context& ctx )
	: ctx_(ctx)
	, book_( ctx.conf_.self_dir )
	, clock_()
	, started_from_root_()
{
	reset();
}

void state_base::reset( position const& p )
{
	p_ = p;
	history_.clear();

	clock_ = 1;

	seen_.reset_root( p.hash_ );

	static uint64_t root_hash = position().hash_;
	started_from_root_ = p_.hash_ == root_hash;

	searchmoves_.clear();

	times_.reset();
}

void state_base::apply( move const& m )
{
	ASSERT( is_valid_move( p_, m, check_map(p_) ) );

	bool reset_seen = false;
	pieces::type piece = p_.get_piece( m );
	pieces::type captured_piece = p_.get_captured_piece( m );
	if( piece == pieces::pawn || captured_piece ) {
		reset_seen = true;
	}

	history_.push_back( std::make_pair( p_, m ) );
	apply_move( p_, m );
	
	if( !reset_seen ) {
		seen_.push_root( p_.hash_ );
	}
	else {
		seen_.reset_root( p_.hash_ );
	}

	++clock_;
	searchmoves_.clear();
}

bool state_base::undo( unsigned int count )
{
	if( !count || count > history_.size() ) {
		return false;
	}

	ASSERT( clock_ >= count );
	clock_ -= count;

	// This isn't exactly correct, if popping past root we would need to restore old seen state prior to a reset.
	seen_.pop_root( count );

	while( --count ) {
		history_.pop_back();
	}
	p_ = history_.back().first;
	history_.pop_back();
	searchmoves_.clear();

	return true;
}

move state_base::get_book_move()
{
	move ret;

	if( ctx_.conf_.use_book && book_.is_open() && clock() < 30 && started_from_root_ ) {
		std::vector<simple_book_entry> moves = book_.get_entries( p() );
		if( !moves.empty() ) {
			short best = moves.front().forecast;
			int count_best = 1;
			for( std::vector<simple_book_entry>::const_iterator it = moves.begin() + 1; it != moves.end(); ++it ) {
				if( it->forecast > -30 && it->forecast + 15 >= best ) {
					++count_best;
				}
			}

			ret = moves[rng_.get_uint64() % count_best].m;
		}
	}

	return ret;
}

bool state_base::recapture() const
{
	bool ret = false;
	
	if( !history().empty() ) {
		position const& old = history().back().first;
		move const& m = history().back().second;
		if( old.get_captured_piece( m ) != pieces::none ) {
			short tmp;
			move best;
			ctx_.tt_.lookup( p().hash_, 0, 0, result::loss, result::win, tmp, best, tmp );
			if( !best.empty() && best.target( ) == m.target() ) {
				ret = true;
			}
		}
	}

	return ret;
}