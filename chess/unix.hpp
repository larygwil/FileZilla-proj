#ifndef __UNIX_H__
#define __UNIX_H__

#include <stdint.h>
#include <pthread.h>
#include <unistd.h>

#define PACKED(c, s) c s __attribute__((__packed__))
#define NONPACKED(s) s

uint64_t timer_precision();
uint64_t get_time();

void console_init();

typedef pthread_rwlock_t rwlock;

class condition;
class mutex {
public:
	mutex();
	~mutex();

private:
	friend class scoped_lock;
	friend class condition;
	pthread_mutex_t m_;
};


class scoped_lock
{
public:
	scoped_lock( mutex& m );
	~scoped_lock();

	void lock();
	void unlock();

private:
	friend class condition;
	mutex& m_;
	bool locked_;
};


class condition
{
public:
	condition();
	~condition();

	void wait( scoped_lock& l );
	void wait( scoped_lock& l, uint64_t timeout );
	void signal( scoped_lock& l );

private:
	bool signalled_;
	pthread_cond_t cond_;
};


class scoped_shared_lock
{
public:
	scoped_shared_lock( rwlock& l );
	~scoped_shared_lock();

private:
	rwlock& l_;
};

class scoped_exclusive_lock
{
public:
	scoped_exclusive_lock( rwlock& l );
	~scoped_exclusive_lock();

private:
	rwlock& l_;
};

void init_rw_lock( rwlock& l );

class thread {
public:
	thread();
	virtual ~thread();

	void spawn();
	void join();

	bool spawned();

	virtual void onRun() = 0;
private:

	pthread_t* t_;
};


unsigned int get_cpu_count();

// In MiB
int get_system_memory();

// Forward bitscan, returns zero-based index of lowest set bit.
// Precondition: mask != 0
inline uint64_t bitscan( uint64_t mask )
{
	// We cannot use the __builtin_ffsll, as it is way more expensive:
	// It always adds 1 to the result, which we then have to subtract again.
	// Not even -O3 can save us there
	uint64_t index;
	asm \
	( \
	"bsfq %[mask], %[index]" \
	:[index] "=r" (index) \
	:[mask] "mr" (mask) \
	);

	return index;
}


// Forward bitscan, returns index of highest set bit.
// Precondition: mask != 0
inline uint64_t bitscan_reverse( uint64_t mask )
{
	uint64_t index;
	asm \
	( \
	"bsrq %[mask], %[index]" \
	:[index] "=r" (index) \
	:[mask] "mr" (mask) \
	);

	return index;
}

#define popcount __builtin_popcountll

#endif
