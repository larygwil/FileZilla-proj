#include "book.hpp"
#include "util.hpp"
#include "zobrist.hpp"
#include "util/mutex.hpp"

#include "sqlite/sqlite3.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

int const db_version = 1;
int const eval_version = 8;

namespace {
unsigned char const table[64] = {
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
  'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
  'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
  'y', 'z', 'A', 'B', 'C', 'D', 'E', 'F',
  'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
  'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
  'W', 'X', 'Y', 'Z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', ',', '.' };


std::string move_to_book_string( move const& m )
{
	std::string ret;
	ret += table[m.source()];
	ret += table[m.target()];

	if( m.promotion() ) {
		switch( m.promotion_piece() ) {
			default:
			case pieces::queen:
				ret += 'q';
				break;
			case pieces::rook:
				ret += 'r';
				break;
			case pieces::bishop:
				ret += 'b';
				break;
			case pieces::knight:
				ret += 'n';
				break;
		}
	}
	else {
		ret += ' ';
	}
	return ret;
}


/*
 * This can only be used to uniquely sort book moves for a given position.
 * Sorting the complete move structure independend of position is not possible
 * With this class. E.g. it doesn't distinguish between capture and non-capture.
 */
class BookMoveSort
{
public:
	BookMoveSort() {}

	bool operator()( move const& lhs, move const& rhs ) const {
		if( lhs.source() < rhs.source() ) {
			return true;
		}
		else if( lhs.source() > rhs.source() ) {
			return false;
		}
		if( lhs.target() < rhs.target() ) {
			return true;
		}
		else if( lhs.target() > rhs.target() ) {
			return false;
		}

		return lhs.promotion_piece() < rhs.promotion_piece();
	}


	bool operator()( move_info const& lhs, move_info const& rhs ) const {
		return operator()( lhs.m, rhs.m );
	}


	bool operator()( book_entry const& lhs, book_entry const& rhs ) const {
		return operator()( lhs.m, rhs.m );
	}
};

BookMoveSort const book_move_sort;
}

std::string book::history_to_string( std::vector<move> const& history )
{
	std::string ret;
	for( std::vector<move>::const_iterator it = history.begin(); it != history.end(); ++it ) {
		ret += move_to_book_string(*it);
	}

	return ret;
}


std::string book::history_to_string( std::vector<move>::const_iterator const& begin, std::vector<move>::const_iterator const& end )
{
	std::string ret;
	for( std::vector<move>::const_iterator it = begin; it != end; ++it ) {
		ret += move_to_book_string(*it);
	}

	return ret;
}


book_entry::book_entry()
	: forecast()
	, search_depth()
	, eval_version()
{
}


class book::impl : public database
{
public:
	impl( std::string const& book_dir )
		: database( book_dir + "opening_book.db" )
	{
	}

	mutex mtx;

	std::ofstream logfile;
};


book::book( std::string const& book_dir )
	: impl_( new impl( book_dir ) )
{
	if( is_open() ) {
		if( impl_->user_version() != db_version ) {
			impl_->close();
		}
	}
}


book::~book()
{
	delete impl_;
}


bool book::is_open() const
{
	scoped_lock l(impl_->mtx);
	return impl_->is_open();
}


void book::close()
{
	scoped_lock l(impl_->mtx);
	impl_->close();
}


bool book::open( std::string const& book_dir )
{
	scoped_lock l(impl_->mtx);
	bool open = impl_->open( book_dir + "opening_book.db" );
	if( open ) {
		if( impl_->user_version() != db_version ) {
			impl_->close();
			open = false;
		}
	}

	return open;
}

namespace {
struct cb_data {
	cb_data()
		: pm()
		, valid_()
		, by_hash_()
	{
	}

	std::vector<book_entry>* entries;
	position p;

	move_info moves[200];
	move_info* pm;

	bool valid_;
	bool by_hash_;
};

unsigned char conv_to_index( unsigned char s )
{
	if( s >= 'a' && s <= 'z' ) {
		return s - 'a';
	}
	else if( s >= 'A' && s <= 'Z' ) {
		return s - 'A' + 26;
	}
	else if( s >= '0' && s <= '9' ) {
		return s - '0' + 26 + 26;
	}
	else if( s == ',' ) {
		return 62;
	}
	else {//if( s == '.' ) {
		return 63;
	}
}


bool conv_to_move_slow( position const& p, move& m, char const* data, bool print_errors ) {
	unsigned char si = conv_to_index( data[0] );
	unsigned char ti = conv_to_index( data[1] );
	
	char ms[6] = {0};
	ms[0] = (si % 8) + 'a';
	ms[1] = (si / 8) + '1';
	ms[2] = (ti % 8) + 'a';
	ms[3] = (ti / 8) + '1';

	switch( data[2] ) {
	case 'q':
		ms[4] = 'q';
		break;
	case 'r':
		ms[4] = 'r';
		break;
	case 'b':
		ms[4] = 'b';
		break;
	case 'n':
		ms[4] = 'n';
		break;
	case ' ':
		break;
	default:
		if( print_errors ) {
			std::cerr << "Invalid promotion character in move history" << std::endl;
		}
		return false;
	}

	std::string error;
	if( !parse_move( p, ms, m, error ) ) {
		if( print_errors ) {
			std::cerr << error << ": " << ms << std::endl;
		}
		return false;
	}

	return true;
}

class data_holder {
public:
	data_holder()
		: d()
		, bytes()
	{
	}

	data_holder( data_holder const& rhs )
		: d()
		, bytes(rhs.bytes)
	{
		if( rhs.d ) {
			d = new unsigned char[ rhs.bytes ];
			memcpy( d, rhs.d, rhs.bytes );
		}
	}

	data_holder( unsigned char const* p, uint64_t b )
		: d()
		, bytes( b )
	{
		if( p ) {
			d = new unsigned char[b];
			memcpy( d, p, b );
		}
	}

	virtual ~data_holder()
	{
		delete [] d;
	}

	data_holder& operator=(data_holder const& rhs)
	{
		if( this == &rhs ) {
			return *this;
		}

		delete [] d;
		if( rhs.d ) {
			d = new unsigned char[ rhs.bytes ];
			memcpy( d, rhs.d, rhs.bytes );
		}
		else {
			d = 0;
		}
		bytes = rhs.bytes;

		return *this;
	}

	bool operator==(data_holder const& rhs) const
	{
		if( bytes != rhs.bytes ) {
			return false;
		}
		return !memcmp( d, rhs.d, bytes );
	}

	bool operator!=(data_holder const& rhs) const
	{
		return !(*this == rhs);
	}

	unsigned char* d;
	uint64_t bytes;
};


bool decode_entries( unsigned char const* data, uint64_t bytes, std::vector<book_entry>& entries )
{
	if( bytes % 4 ) {
		return false;
	}

	uint64_t size = bytes / 4;

	for( unsigned int i = 0; i < size; ++i ) {
		book_entry e;
		unsigned short forecast = *(data++);
		forecast += static_cast<unsigned short>(*(data++)) * 256;
		e.forecast = static_cast<short>(forecast);

		e.search_depth = *(data++);
		e.eval_version = *(data++);

		entries.push_back(e);
	}

	return true;
}


bool encode_entries( std::vector<book_entry>& entries, data_holder& data, bool set_current_eval_version = false )
{
	uint64_t bytes = entries.size() * 4;
	data = data_holder( new unsigned char[bytes], bytes );

	unsigned char* p = data.d;
	for( std::size_t i = 0; i < entries.size(); ++i ) {
		*(p++) = static_cast<unsigned short>(entries[i].forecast) % 256;
		*(p++) = static_cast<unsigned short>(entries[i].forecast) / 256;
		*(p++) = entries[i].search_depth;
		if( set_current_eval_version ) {
			*(p++) = eval_version;
		}
		else {
			*(p++) = static_cast<unsigned char>(entries[i].eval_version);
		}
	}

	return true;
}

extern "C" int get_cb( void* p, sqlite3_stmt* statement )
{
	cb_data* d = reinterpret_cast<cb_data*>(p);
	if( d->valid_ ) {
		return 0;
	}

	if( !statement ) {
		std::cerr << "No prepared statement passed" << std::endl;
		return 1;
	}
	if( sqlite3_column_count( statement ) != 1 ) {
		std::cerr << "Wrong column count" << std::endl;
		return 1;
	}
	if( sqlite3_column_type( statement, 0 ) == SQLITE_NULL ) {
		return 0;
	}

	uint64_t bytes = sqlite3_column_bytes( statement, 0 );
	if( bytes % 4 ) {
		std::cerr << "Wrong data size not multiple of 4: " << bytes << std::endl;
		return 1;
	}
	uint64_t size = bytes / 4;

	unsigned char const* data = reinterpret_cast<unsigned char const*>(sqlite3_column_blob( statement, 0 ) );
	if( !data ) {
		std::cerr << "No data" << std::endl;
		return 1;
	}

	if( !d->pm ) {
		check_map check( d->p );
		d->pm = d->moves;
		calculate_moves( d->p, d->pm, check );

		std::sort( d->moves, d->pm, book_move_sort );
	}

	if( size != static_cast<uint64_t>(d->pm - d->moves) ) {
		if( d->by_hash_ ) {
			std::cerr << "Possible hash collision" << std::endl;
			return 0;
		}
		else {
			std::cerr << "Corrupt book entry, expected " << (d->pm - d->moves) << " moves, got " << size << " moves." << std::endl;
			return 1;
		}
	}

	if( !decode_entries( data, bytes, *d->entries ) ) {
		std::cerr << "Could not decode entries" << std::endl;
		return 1;
	}

	for( unsigned int i = 0; i < size; ++i ) {
		(*d->entries)[i].m = d->moves[i].m;
	}

	std::sort( d->entries->begin(), d->entries->end() );

	d->valid_ = true;

	return 0;
}
}


std::vector<book_entry> book::get_entries( position const& p, std::vector<move> const& history, bool allow_transpositions )
{
	std::vector<book_entry> ret;

	{
		scoped_lock l(impl_->mtx);

		if( !impl_->is_open() ) {
			return ret;
		}

		cb_data data;
		data.p = p;
		data.entries = &ret;

		std::string hs = history_to_string( history );

		std::stringstream ss;
		ss << "SELECT data FROM position WHERE pos = '" << hs << "'";

		bool success = impl_->query_row( ss.str(), &get_cb, reinterpret_cast<void*>(&data) );
		if( !success || !data.valid_ ) {
			ret.clear();
		}
	}

	if( ret.empty() && allow_transpositions ) {
		ret = get_entries( p );
	}

	return ret;
}


std::vector<book_entry> book::get_entries( position const& p )
{
	std::vector<book_entry> ret;

	scoped_lock l(impl_->mtx);

	if( !impl_->is_open() ) {
		return ret;
	}

	cb_data data;
	data.p = p;
	data.entries = &ret;
	data.by_hash_ = true;

	uint64_t hash = get_zobrist_hash( p );
	
	std::stringstream ss;
	ss << "SELECT data FROM position WHERE hash=" << static_cast<sqlite3_int64>(hash);

	bool success = impl_->query_row( ss.str(), &get_cb, reinterpret_cast<void*>(&data), true );
	if( !success || !data.valid_ ) {
		ret.clear();
	}
	else {
		std::cerr << "Found entry by transposition" << std::endl;
	}

	return ret;
}


namespace {
extern "C" int get_data_cb( void* p, sqlite3_stmt* statement )
{
	data_holder* d = reinterpret_cast<data_holder*>(p);

	if( !statement || sqlite3_column_count( statement ) != 1 ) {
		return 1;
	}
	if( sqlite3_column_type( statement, 0 ) == SQLITE_NULL ) {
		return 0;
	}

	uint64_t bytes = sqlite3_column_bytes( statement, 0 );
	if( bytes % 4 ) {
		return 1;
	}

	if( bytes ) {
		unsigned char const* data = reinterpret_cast<unsigned char const*>(sqlite3_column_blob( statement, 0 ) );
		if( !data ) {
			return 1;
		}

		*d = data_holder( data, bytes );
	}
	else {
		*d = data_holder();
	}

	return 0;
}


bool get_position( std::string history, position& p )
{
	if( history.size() % 3 ) {
		return false;
	}

	p.reset();

	while( !history.empty() ) {
		std::string ms = history.substr( 0, 3 );
		history = history.substr( 3 );

		move m;
		if( !conv_to_move_slow( p, m, ms.c_str(), true ) ) {
			return false;
		}
		apply_move( p, m );
	}

	return true;
}


int do_fold_position( void* q, sqlite3_stmt* statement, bool verify )
{
	book::impl* impl_ = reinterpret_cast<book::impl*>(q);

	if( !statement || sqlite3_column_count(statement) != 2 ) {
		std::cerr << "Wrong column count" << std::endl;
		return 1;
	}

	unsigned char const* pos = sqlite3_column_text( statement, 0 );
	if( !pos ) {
		std::cerr << "No move history returned" << std::endl;
		return 1;
	}
	std::string const history = reinterpret_cast<char const*>(pos);
	if( history.size() % 3 ) {
		std::cerr << "Move history malformed" << std::endl;
		return 1;
	}
	if( !history.size() ) {
		return 0;
	}

	if( sqlite3_column_type( statement, 1 ) == SQLITE_NULL ) {
		std::cerr << "NULL data in position to fold." << std::endl;
		return 1;
	}

	uint64_t bytes = sqlite3_column_bytes( statement, 1 );
	if( bytes % 4 ) {
		std::cerr << "Data has wrong size" << std::endl;
		return 1;
	}

	unsigned char const* data = reinterpret_cast<unsigned char const*>(sqlite3_column_blob( statement, 1 ) );
	if( !data ) {
		std::cerr << "No data in non-NULL column" << std::endl;
		return 1;
	}

	position p;
	if( !get_position( history, p ) ) {
		std::cerr << "Could not get position from move history" << std::endl;
		return 1;
	}

	check_map const check( p );

	short forecast = result::loss;
	unsigned char depth = 0;
	if( !bytes ) {
		// Mate or draw.
		if( !check.check ) {
			forecast = 0;
		}
	}
	else {
		std::vector<book_entry> entries;
		if( !decode_entries( data, bytes, entries ) ) {
			std::cerr << "Could not decode entries of position" << std::endl;
			return 1;
		}

		if( verify ) {
			move_info moves[200];
			move_info* pm;

			pm = moves;
			calculate_moves( p, pm, check );

			std::sort( moves, pm, book_move_sort );

			if( entries.size() != static_cast<uint64_t>(pm - moves) ) {
				std::cerr << "Corrupt book entry, expected " << (pm - moves) << " moves, got " << entries.size() << " moves." << std::endl;
				return 1;
			}

			for( unsigned int i = 0; i < entries.size(); ++i ) {
				entries[i].m = moves[i].m;
			}
		}

		for( std::size_t i = 0; i < entries.size(); ++i ) {
			if( entries[i].forecast > forecast ) {
				forecast = entries[i].forecast;
				depth = entries[i].search_depth;
			}
			else if( entries[i].forecast == forecast && entries[i].search_depth < depth ) {
				depth = entries[i].search_depth;
			}
			if( verify && entries[i].eval_version != 0 ) {
				std::string chs = history + move_to_book_string( entries[i].m );
				position cp;
				if( !get_position( chs, cp ) ) {
					std::cerr << "Corrupt book, child node not found." << std::endl;
					return 1;
				}
			}
		}
	}
	forecast = -forecast;
	++depth;

	// We now got the best forecast of the current position and its depth
	// Get parent position:
	std::string parent = history.substr( 0, history.size() - 3 );
	std::string ms = history.substr( history.size() - 3 );

	data_holder dh;
	if( !impl_->query_row( "SELECT data FROM position WHERE pos = '" + parent + "'", &get_data_cb, reinterpret_cast<void*>(&dh) ) ) {
		return 1;
	}
	if( !dh.d || !dh.bytes ) {
		if( verify ) {
			std::cerr << "Error in book, position has no parent: " << history << std::endl;
			return 1;
		}
		else {
			// Can't fold yet. This situation can happen if processing a tree with multiple threads.
			return 0;
		}
	}

	position pp;
	if( !get_position( parent, pp ) ) {
		return 1;
	}

	move_info moves[200];
	move_info* it = moves;
	calculate_moves( pp, it, check_map( pp ) );
	if( static_cast<uint64_t>(it - moves) != dh.bytes / 4 ) {
		std::cerr << "Wrong move count in parent position's data: " << (it - moves) << " " << dh.bytes / 4 << std::endl;
		return 1;
	}
	std::sort( moves, it, book_move_sort );

	move m;
	if( !conv_to_move_slow( pp, m, ms.c_str(), true ) ) {
		std::cerr << "Could not get move" << std::endl;
		return 1;
	}

	int i = 0;
	for( ; i < it - moves; ++i ) {
		if( moves[i].m == m ) {
			break;
		}
	}
	if( i == it - moves ) {
		std::cerr << "Target move not found in all valid parent moves" << std::endl;
		return 1;
	}

	data_holder new_dh = dh;
	new_dh.d[i*4] = static_cast<unsigned short>(forecast) % 256;
	new_dh.d[i*4 + 1] = static_cast<unsigned short>(forecast) / 256;
	new_dh.d[i*4 + 2] = depth;
	new_dh.d[i*4 + 3] = 0;

	if( new_dh != dh ) {
		if( !impl_->query_bind( "UPDATE position SET data = :1 WHERE pos = '" + parent + "'", new_dh.d, new_dh.bytes ) ) {
			return 1;
		}
	}

	return 0;
}

extern "C" int fold_position( void* q, sqlite3_stmt* statement )
{
	return do_fold_position( q, statement, false );
}

extern "C" int verify_position( void* q, sqlite3_stmt* statement )
{
	return do_fold_position( q, statement, true );
}
}


bool book::add_entries( std::vector<move> const& history, std::vector<book_entry> entries )
{
	if( !is_writable() ) {
		std::cerr << "Cannot add entries to read-only book" << std::endl;
		return false;
	}

	std::sort( entries.begin(), entries.end(), book_move_sort );

	std::string hs = history_to_string( history );

	position p;

	for( std::vector<move>::const_iterator it = history.begin(); it != history.end(); ++it ) {
		apply_move( p, *it );
	}
	uint64_t hash = get_zobrist_hash( p );

	scoped_lock l(impl_->mtx);

	impl_->logfile << "BEGIN TRANSACTION;";
	if( !impl_->query("BEGIN TRANSACTION", 0, 0 ) ) {
		return false;
	}
	std::stringstream ss;
	ss << "INSERT OR REPLACE INTO position (pos, hash, data) VALUES ('" << hs << "', " << static_cast<sqlite3_int64>(hash) << ", :1);";
	impl_->logfile << ss.str();
	impl_->logfile.flush();

	data_holder dh;
	encode_entries( entries, dh, true );
	if( !impl_->query_bind( ss.str(), dh.d, dh.bytes ) ) {
		return false;
	}

	for( std::vector<book_entry>::const_iterator it = entries.begin(); it != entries.end(); ++it ) {
		std::stringstream ss;
		ss << "SELECT pos, data FROM position WHERE pos='" << hs + move_to_book_string( it->m ) << "' AND data IS NOT NULL;";
		if( !impl_->query_row( ss.str(), &fold_position, impl_ ) ) {
			return false;
		}
	}

	while( hs.size() >= 3 ) {
		std::stringstream ss;
		ss << "SELECT pos, data FROM position WHERE pos='" << hs << "' AND data IS NOT NULL;";
		if( !impl_->query_row( ss.str(), &fold_position, impl_ ) ) {
			return false;
		}
		hs = hs.substr( 0, hs.size() - 3 );
	}
	return impl_->query( "COMMIT TRANSACTION;", 0, 0 );
}


namespace {
extern "C" int count_cb( void* p, int, char** data, char** /*names*/ ) {
	uint64_t* count = reinterpret_cast<uint64_t*>(p);
	*count = atoll( *data );

	return 0;
}
}


uint64_t book::size()
{
	scoped_lock l(impl_->mtx);

	if( !impl_->is_open() ) {
		return 0;
	}

	std::string query = "SELECT COUNT(pos) FROM position;";

	uint64_t count = 0;
	impl_->query( query, &count_cb, reinterpret_cast<void*>(&count) );

	return count;
}


bool book::mark_for_processing( std::vector<move> const& history )
{
	scoped_lock l(impl_->mtx);

	if( !impl_->is_open() || !impl_->is_writable() ) {
		return false;
	}

	std::stringstream ss;

	ss << "BEGIN TRANSACTION;";

	position p;

	for( std::vector<move>::const_iterator it = history.begin(); it != history.end(); ++it ) {
		apply_move( p, *it );
		uint64_t hash = get_zobrist_hash( p );

		std::string hs = history_to_string( history.begin(), it + 1 );
		ss << "INSERT OR IGNORE INTO position (pos, hash) VALUES ('" << hs << "', " << static_cast<sqlite3_int64>(hash) << ");";
	}

	ss << "COMMIT TRANSACTION;";

	impl_->query( ss.str(), 0, 0 );

	return true;
}


namespace {
extern "C" int work_cb( void* p, int, char** data, char** /*names*/ )
{
	std::list<work>* wl = reinterpret_cast<std::list<work>*>(p);

	std::string pos = *data;
	if( pos.length() % 3 ) {
		return 1;
	}

	work w;
	w.seen.reset_root( get_zobrist_hash(w.p) );

	while( !pos.empty() ) {
		std::string ms = pos.substr( 0, 3 );
		pos = pos.substr( 3 );

		move m;
		if( !conv_to_move_slow( w.p, m, ms.c_str(), true ) ) {
			return 1;
		}

		apply_move( w.p, m );
		w.move_history.push_back( m );
		w.seen.push_root( get_zobrist_hash(w.p) );
	}

	wl->push_back( w );

	return 0;
}
}


std::list<work> book::get_unprocessed_positions()
{
	std::list<work> ret;

	scoped_lock l(impl_->mtx);

	std::string query = "SELECT pos FROM position WHERE data IS NULL ORDER BY LENGTH(pos) ASC;";

	impl_->query( query, &work_cb, reinterpret_cast<void*>(&ret) );

	return ret;
}


bool book::redo_hashes()
{
	if( !impl_->is_writable() ) {
		std::cerr << "Error: Cannot redo hashes on read-only opening book\n" << std::endl;
		return false;
	}

	std::list<work> positions;

	{
		scoped_lock l(impl_->mtx);

		std::string query = "SELECT pos FROM position ORDER BY LENGTH(pos) DESC";

		impl_->query( query, &work_cb, reinterpret_cast<void*>(&positions) );
	}

	std::stringstream ss;

	ss << "BEGIN TRANSACTION;";
	for( std::list<work>::const_iterator it = positions.begin(); it != positions.end(); ++it ) {
		std::string hs = history_to_string( it->move_history );
		uint64_t hash = get_zobrist_hash( it->p );
		ss << "UPDATE position SET hash = " << static_cast<sqlite_int64>(hash) << " WHERE pos='" << hs << "';";
	}

	ss << "COMMIT TRANSACTION;";
	impl_->query( ss.str(), 0, 0 );

	return true;
}


std::list<book_entry_with_position> book::get_all_entries()
{
	std::list<book_entry_with_position> ret;

	std::list<work> positions;

	{
		scoped_lock l(impl_->mtx);

		std::string query = "SELECT pos FROM position ORDER BY LENGTH(pos) DESC";

		impl_->query( query, &work_cb, reinterpret_cast<void*>(&positions) );
	}

	for( std::list<work>::const_iterator it = positions.begin(); it != positions.end(); ++it ) {
		book_entry_with_position entry;
		entry.w = *it;
		entry.entries = get_entries( it->p, it->move_history );

		if( !entry.entries.empty() ) {
			ret.push_back( entry );
		}
	}

	return ret;
}


bool book::update_entry( std::vector<move> const& history, book_entry const& entry )
{
	std::string hs = history_to_string( history );

	scoped_lock l(impl_->mtx);

	transaction t( *impl_ );
	if( !t.init() ) {
		return false;
	}

	data_holder dh;
	if( !impl_->query_row( "SELECT data FROM position WHERE pos = '" + hs + "'", &get_data_cb, reinterpret_cast<void*>(&dh) ) ) {
		return false;
	}
	if( !dh.bytes ) {
		std::cerr << "Cannot update empty entry" << std::endl;
		return false;
	}

	position p;
	if( !get_position( hs, p ) ) {
		return false;
	}

	move_info moves[200];
	move_info* it = moves;
	calculate_moves( p, it, check_map( p ) );
	if( static_cast<uint64_t>(it - moves) != dh.bytes / 4 ) {
		std::cerr << "Wrong move count in position's data: " << (it - moves) << " " << dh.bytes / 4 << std::endl;
		return false;
	}
	std::sort( moves, it, book_move_sort );

	int i = 0;
	for( ; i < it - moves; ++i ) {
		if( moves[i].m == entry.m ) {
			break;
		}
	}
	if( i == it - moves ) {
		std::cerr << "Target move not found in all valid moves" << std::endl;
		return false;
	}

	if( !dh.d[i*4+3] ) {
		std::cerr << "Cannot update folded entry" << std::endl;
		return false;
	}

	data_holder new_dh = dh;
	new_dh.d[i*4] = static_cast<unsigned short>(entry.forecast) % 256;
	new_dh.d[i*4 + 1] = static_cast<unsigned short>(entry.forecast) / 256;
	new_dh.d[i*4 + 2] = entry.search_depth;
	new_dh.d[i*4 + 3] = eval_version;

	if( dh != new_dh ) {
		if( !impl_->query_bind( "UPDATE position SET data = :1 WHERE pos = '" + hs + "'", new_dh.d, new_dh.bytes ) ) {
			return 1;
		}

		hs += move_to_book_string( entry.m );

		while( hs.size() >= 3 ) {
			std::stringstream ss;
			ss << "SELECT pos, data FROM position WHERE pos='" << hs << "' AND data IS NOT NULL;";
			if( !impl_->query_row( ss.str(), &fold_position, impl_ ) ) {
				return false;
			}
			hs = hs.substr( 0, hs.size() - 3 );
		}
	}

	return t.commit();
}

bool book::fold( bool verify )
{
	scoped_lock l(impl_->mtx);

	if( !impl_->is_writable() ) {
		std::cerr << "Error: Cannot fold read-only opening book\n" << std::endl;
		return false;
	}

	uint64_t max_length = 0;
	std::string query = "BEGIN TRANSACTION; SELECT LENGTH(pos) FROM position ORDER BY LENGTH(pos) DESC LIMIT 1;";
	if( !impl_->query( query, &count_cb, &max_length ) ) {
		return false;
	}

	std::cerr << "Folding";
	for( uint64_t i = max_length; i > 0; i -= 3 ) {
		std::cerr << ".";
		std::stringstream ss;
		ss << "SELECT pos, data FROM position WHERE length(pos) = " << i << " AND data IS NOT NULL;";
		if( verify ) {
			impl_->query_row( ss.str(), &verify_position, impl_ );
		}
		else {
			impl_->query_row( ss.str(), &fold_position, impl_ );
		}
	}
	std::cerr << " done" << std::endl;

	std::cerr << "Comitting...";
	query = "COMMIT TRANSACTION;";
	if( !impl_->query( query, 0, 0 ) ) {
		return false;
	}
	std::cerr << " done" << std::endl;

	return true;
}


namespace {
extern "C" int stats_processed_cb( void* p, int, char** data, char** /*names*/ ) {
	book_stats* stats = reinterpret_cast<book_stats*>(p);

	int64_t depth = atoll( data[0] ) / 3;
	int64_t processed = atoll( data[1] );

	if( depth > 0 && processed > 0 ) {
		stats->data[depth].processed = static_cast<uint64_t>(processed);
		stats->total_processed += static_cast<uint64_t>(processed);
	}

	return 0;
}

extern "C" int stats_queued_cb( void* p, int, char** data, char** /*names*/ ) {
	book_stats* stats = reinterpret_cast<book_stats*>(p);

	int64_t depth = atoll( data[0] ) / 3;
	int64_t queued = atoll( data[1] );

	if( depth > 0 && queued > 0 ) {
		stats->data[depth].queued = static_cast<uint64_t>(queued);
		stats->total_queued += static_cast<uint64_t>(queued);
	}

	return 0;
}
}

book_stats book::stats()
{
	book_stats ret;

	scoped_lock l(impl_->mtx);

	std::string query = "SELECT length(pos), count(pos), SUM(LENGTH(data)/4) FROM position WHERE data is NOT NULL GROUP BY LENGTH(pos) ORDER BY LENGTH(pos)";
	impl_->query( query, &stats_processed_cb, reinterpret_cast<void*>(&ret) );
	
	query = "SELECT length(pos), count(pos), SUM(LENGTH(data)/4) FROM position WHERE data is NULL GROUP BY LENGTH(pos) ORDER BY LENGTH(pos)";
	impl_->query( query, &stats_queued_cb, reinterpret_cast<void*>(&ret) );

	return ret;
}


bool book::set_insert_logfile( std::string const& log_file )
{
	scoped_lock l(impl_->mtx);

	impl_->logfile.close();
	if( !log_file.empty() ) {
		impl_->logfile.open( log_file.c_str(), std::ofstream::out|std::ofstream::app );
	}

	return impl_->logfile.is_open();
}


std::string entries_to_string( position const& p, std::vector<book_entry> const& entries )
{
	std::ostringstream out;
	out << "  Move     Forecast   In book" << std::endl;
	for( std::vector<book_entry>::const_iterator it = entries.begin(); it != entries.end(); ++it ) {
		out << std::setw(6) << move_to_san( p, it->m )
			<< std::setw(7) << it->forecast << " @ " << std::setw(2) << static_cast<int>(it->search_depth) << std::setw(6) << (it->is_folded() ? "yes" : "")
			<< std::endl;
	}

	return out.str();
}

bool book::is_writable() const
{
	return impl_->is_writable();
}
