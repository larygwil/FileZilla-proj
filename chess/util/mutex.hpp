#ifndef __MUTEX_HPP__
#define __MUTEX_HPP__

#include "time.hpp"

class mutex
{
	class impl;
public:
	mutex();
	~mutex();

	mutex( mutex const& ) = delete;
	mutex& operator=( mutex const& ) = delete;

private:
	friend class scoped_lock;
	friend class condition;
	impl* impl_;
};


class scoped_lock
{
public:
	explicit scoped_lock( mutex& m );
	~scoped_lock();

	scoped_lock( scoped_lock const& ) = delete;
	scoped_lock& operator=( scoped_lock const& ) = delete;

	void lock();
	void unlock();

private:
	friend class condition;
	mutex& m_;
	bool locked_;
};


class condition
{
	class impl;
public:
	condition();
	~condition();

	condition( condition const& ) = delete;
	condition& operator=( condition const& ) = delete;
	
	void wait( scoped_lock& l );
	// Milliseconds
	void wait( scoped_lock& l, duration const& timeout );
	void signal( scoped_lock& l );

private:
	impl* impl_;	
};

#endif
