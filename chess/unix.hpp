#ifndef __UNIX_H__
#define __UNIX_H__

#include <stdint.h>
#include <pthread.h>

#define PACKED(s) s __attribute__((__packed__))

unsigned long long timer_precision();
unsigned long long get_time();

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

private:
	friend class condition;
	mutex& m_;
};


class condition
{
public:
	condition();
	~condition();

	void wait( scoped_lock& l );
	void wait( scoped_lock& l, unsigned long long timeout );
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
	~thread();

	void spawn();
	void join();

	bool spawned();

	virtual void onRun() = 0;
private:

	pthread_t* t_;
};

#define bitscan __builtin_ffsll
#define popcount __builtin_popcountll

#endif
