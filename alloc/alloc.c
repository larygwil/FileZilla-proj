/*
 * Copyright (C) 2010 - Tim Kosse <tim.kosse@filezilla-project.org>
 *
 * This file is licensed under the terms and conditions of the GNU General
 * Public License version 2 or later.
 *
 * 1) Features:
 *
 * 1.1) Guarding against access to unallocated memory
 *
 *      When calling free(), all associated pages are set to be inaccessible
 *      and unreadable such that any access to these pages will result in a
 *      segmentation fault.
 *      To improve detection rates, all previously free'ed pages will not be
 *      reused for some time in further allocations.
 *
 * 1.2) Guarding against buffer overruns
 *
 *      After each allocated buffer a guard page is added that is neither
 *	    readable nor writable. Also, if the allocated buffer is at least
 *      4 bytes smaller than the page size, a canary is added after the end
 *      of the buffer.
 *
 * 1.3) Guarding against buffer underruns
 *
 *      The page before the allocated buffer is marked read and write
 *      protected. In addition, a canary protects the page containing
 *      metadata.
 *
 * 2) Requirements:
 *
 *      A recent Linux distribution and GCC 4.1 or later. This tool
 *      has only been tested on the AMD64 platform.
 *
 *
 * 3) Usage:
 *
 *     export LD_PRELOAD=/path/to/liballoc.so
 *
 *     You may have to increase the vm.max_map_count system option. As
 *     superuser, execute the following command:
 *     sysctl -w vm.max_map_count=500000000
 *
 *     Please note that for every allocated block of memory, at least 3
 *     entire memory pages are used, so memory requirements are a lot higher
 *     if using this tool than without.
 *
 */

#define BUCKETS 16
#define PER_BUCKET_MINFREE 200 // In allocked blocks
#define OVERALL_MINFREE 500000 // In pages. 500000 is 2GB at 4k pagesize.

#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>

static volatile int locked = 0;

volatile int malloc_count = 0;
volatile int free_count = 0;

volatile unsigned int canary;

void lock()
{
	while (!__sync_bool_compare_and_swap( &locked, 0, 1 ) ) {
		// Keep trying
	}
}

void unlock()
{
	locked = 0;
	__sync_synchronize();
}

struct _info
{
	int pages; // >= 1
	int orig_pages; // >= pages at initial malloc
	int user_size; // Current user size after realloc and such

	int is_free;

	unsigned int canary;

	void *next_free; // Only set if in the free list
};

struct _free
{
	struct _info* first;
	struct _info* last;
	int count;
};

// Free pages, sorted by size
volatile struct _free free_pages[BUCKETS + 1] = {0};

struct _info* reclaim_free( int pages )
{
	int i = pages - 2;
	if( i < 0 || i >= BUCKETS ) {
		return 0;
	}

	lock();

	if( free_count < OVERALL_MINFREE ) {
		unlock();
		return 0;
	}

	struct _info* p = 0;
	for( ; i < BUCKETS; ++i ) {
		if( free_pages[i].count > PER_BUCKET_MINFREE ) {
			p = free_pages[i].first;
			mprotect( p, getpagesize(), PROT_READ | PROT_WRITE );

			free_pages[i].first = p->next_free;
			if( !p->next_free ) {
				free_pages[i].last = 0;
			}
			--free_pages[i].count;
			free_count -= p->orig_pages;

			break;
		}
	}

	unlock();

	return p;
}

void* malloc( size_t size )
{
	int pages = ( size + getpagesize() - 1 ) / getpagesize() + 2;

	void* p = reclaim_free( pages );
	if( !p ) {
		// Info page + user data rounded up to next page boundary, guard page
		p = mmap( 0, getpagesize() * pages, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0 );
		if( !p) {
			abort();
		}

		// Guard page behind
		mprotect( p + (pages - 1) * getpagesize(), getpagesize(), PROT_NONE );

		struct _info *i = (struct _info*)p;
		i->orig_pages = pages;

		i->canary = __sync_fetch_and_add( &canary, 1 );
		*(int*)(p + getpagesize() - sizeof(int) ) = i->canary;
	}
	else {
		mprotect( p + getpagesize(), (pages - 2) * getpagesize(), PROT_READ | PROT_WRITE );
	}

	struct _info *i = (struct _info*)p;
	i->pages = pages;
	i->user_size = size;
	i->is_free = 0;
	i->next_free = 0;

	if( (i->pages - 2) * getpagesize() - i->user_size >= 4 ) {
		*(int*)(p + getpagesize() + i->user_size ) = i->canary;
	}

	// Info page before, unwritable
	mprotect( p, getpagesize(), PROT_NONE );

	int count = __sync_add_and_fetch( &malloc_count, 1 );

	return p + getpagesize();
}

void* calloc( size_t nmemb, size_t size )
{
	void* p = malloc( size * nmemb );

	memset( p, 0, size * nmemb );

	return p;
}

static void check_sanity( struct _info* i ) {
	if( i->is_free ) {
		abort();
	}
	if( i->user_size > (i->pages - 2) * getpagesize() ) {
		abort();
	}
	if( i->pages > i->orig_pages ) {
		abort();
	}

	if( *(int*)((void*)i + getpagesize() - sizeof(int) ) != i->canary ) {
		abort();
	}

	if( (i->pages - 2) * getpagesize() - i->user_size >= 4 ) {
		if( *(int*)((void*)i + getpagesize() + i->user_size ) != i->canary ) {
			abort();
		}
	}
}

void* realloc( void* old, size_t size )
{
	if( !old ) {
		return malloc(size);
	}
	if( !size ) {
		abort();
	}

	// Round down to nearest page boundary
	void* aligned_old = old - (intptr_t)old % getpagesize();

	mprotect( aligned_old - getpagesize(), getpagesize(), PROT_READ );

	struct _info *oldinfo = (struct _info*)(aligned_old - getpagesize() );

	check_sanity( oldinfo );

	void* p;
	if( size <= oldinfo->user_size ) {
		p = old;
		mprotect( aligned_old - getpagesize(), getpagesize(), PROT_WRITE | PROT_READ );
		oldinfo->user_size = size;
		if( (oldinfo->pages - 2) * getpagesize() - oldinfo->user_size >= 4 ) {
			*(int*)(aligned_old + oldinfo->user_size ) = oldinfo->canary;
		}

		mprotect( aligned_old - getpagesize(), getpagesize(), PROT_NONE );
	}
	else {
		p = malloc( size );

		int cp;
		if( oldinfo->user_size > size ) {
			cp = size;
		}
		else {
			cp = oldinfo->user_size;
		}
		memcpy( p, old, cp );

		free( old );
	}

	return p;
}

static void add_to_free( volatile struct _free *f, struct _info *i ) {
	i->next_free = 0;
	i->is_free = 1;
	free_count += i->orig_pages;
	mprotect( i, getpagesize() * i->pages - 1, PROT_NONE );
	if( f->last ) {
		mprotect( f->last, getpagesize(), PROT_WRITE );
		f->last->next_free = i;
		mprotect( f->last, getpagesize(), PROT_NONE );
	}
	f->last = i;
	if( !f->first ) {
		f->first = i;
	}
	++f->count;
}

void free( void* p )
{
	if( !p ) {
		return;
	}

	__sync_sub_and_fetch( &malloc_count, 1 );

	char* q = p;

	// Round down to nearest page boundary
	q -= (intptr_t)q % getpagesize();

	// Get info page
	q -= getpagesize();

	struct _info *i = (struct _info*)q;

	mprotect( q, getpagesize(), PROT_READ|PROT_WRITE );

	check_sanity( i );

	lock();

	int bucket = i->orig_pages - 2;
	if( bucket < 0 || bucket > BUCKETS ) {
		add_to_free( &free_pages[BUCKETS], i );
		if( free_pages[BUCKETS].count > PER_BUCKET_MINFREE ) {
			i = free_pages[BUCKETS].first;
			mprotect( i, getpagesize(), PROT_READ);
			free_pages[BUCKETS].first = i->next_free;
			free_count -= i->orig_pages;
			if( !i->next_free) {
				free_pages[BUCKETS].last = 0;
			}
			munmap( i, i->orig_pages * getpagesize() );
			--free_pages[BUCKETS].count;
		}
	}
	else {
		add_to_free( &free_pages[bucket], i );
	}

	unlock();
}

int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	if( alignment <= getpagesize() ) {

		if( getpagesize() % alignment ) {
			abort();
		}

		*memptr = malloc( size );
	}
	else {
		if( alignment % getpagesize() ) {
			abort();
		}

		int pages = ( size + getpagesize() - 1 ) / getpagesize() + 2;

		int extra_pages = (getpagesize() % alignment) / getpagesize();

		// Info page + user data rounded up to next page boundary, guard page
		void* p = mmap( 0, getpagesize() * (pages + extra_pages), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0 );
		if( !p) {
			abort();
		}

		int offset_pages = ( (intptr_t)(p + getpagesize() ) % alignment ) / getpagesize();

		if( offset_pages ) {
			munmap( p, offset_pages * getpagesize() );
		}
		if( extra_pages - offset_pages ) {
			munmap( p + (pages + offset_pages) * getpagesize(), (extra_pages - offset_pages) * getpagesize() );
		}
		p += offset_pages * getpagesize();

		// Guard page behind
		mprotect( p + (pages - 1) * getpagesize(), getpagesize(), PROT_NONE );

		struct _info *i = (struct _info*)p;
		i->pages = pages;
		i->orig_pages = pages;
		i->user_size = size;
		i->is_free = 0;
		i->next_free = 0;

		i->canary = __sync_fetch_and_add( &canary, 1 );
		*(int*)(p + getpagesize() - sizeof(int) ) = i->canary;

		if( (i->pages - 2) * getpagesize() - i->user_size >= 4 ) {
			*(int*)(p + getpagesize() + i->user_size ) = i->canary;
		}

		// Info page before, unwritable
		mprotect( p, getpagesize(), PROT_NONE );

		if( ((intptr_t)p + getpagesize() ) % alignment ) {
			abort();
		}
		*memptr = p + getpagesize();
	}

	return 0;
}

void *valloc(size_t size)
{
	return malloc(size);
}

void *memalign(size_t boundary, size_t size)
{
	void* p = 0;
	if( posix_memalign( &p, boundary, size ) ) {
		p = 0;
	}
	return p;
}
