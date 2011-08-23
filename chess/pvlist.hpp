#ifndef __PVLIST_H__
#define __PVLIST_H__

#include "chess.hpp"

#include <string>

/*
 * Special list implementation to hold the principal variation.
 * The pool only actually deallocates entries when the pool is
 * destroyed to maximize efficiency.
 */

class pv_entry {
public:
	pv_entry* next() const { return next_; }

	move const& get_best_move() const { return best_move_; }

private:
	move best_move_;

	pv_entry();

	friend class pv_entry_pool;
	pv_entry* next_;
};

class pv_entry_pool {
public:
	pv_entry_pool();
	~pv_entry_pool();

	pv_entry* get();
	void release( pv_entry* pv );

	void append( pv_entry* parent, move const& best_move, pv_entry* child );

	void set_pv_move( pv_entry* pv, move const& m );
	void clear_pv_move( pv_entry* pv );

private:
	pv_entry* first_free_;
	pv_entry* last_free_;
};

void print_pv( pv_entry const* pv, position p, color::type c );
std::string pv_to_string( pv_entry const* pv, position p, color::type c );

#endif
