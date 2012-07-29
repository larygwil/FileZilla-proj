#include "mutex.hpp"

#include <pthread.h>
#include <sys/time.h>

class mutex::impl
{
public:
	impl()
	{
		pthread_mutex_init( &m_, 0 );
	}

	~impl()
	{
		pthread_mutex_destroy( &m_ );
	}

public:
	pthread_mutex_t m_;
};


mutex::mutex()
	: impl_(new impl())
{
}


mutex::~mutex()
{
	delete impl_;
}


scoped_lock::scoped_lock( mutex& m )
	: m_(m)
{
	pthread_mutex_lock( &m.impl_->m_ );
	locked_ = true;
}


scoped_lock::~scoped_lock()
{
	if( locked_ ) {
		pthread_mutex_unlock( &m_.impl_->m_ );
	}
}


void scoped_lock::lock()
{
	pthread_mutex_lock( &m_.impl_->m_ );
	locked_ = true;
}


void scoped_lock::unlock()
{
	pthread_mutex_unlock( &m_.impl_->m_ );
	locked_ = false;
}


class condition::impl
{
public:
	impl()
		: signalled_()
	{
		pthread_cond_init( &cond_, 0 );
	}

	~impl()
	{
		pthread_cond_destroy( &cond_ );
	}

	pthread_cond_t cond_;
	bool signalled_;
};


condition::condition()
	: impl_(new impl())
{
}


condition::~condition()
{
	delete impl_;
}


void condition::wait( scoped_lock& l )
{
	if( impl_->signalled_ ) {
		impl_->signalled_ = false;
		return;
	}
	int res;
	do {
		res = pthread_cond_wait( &impl_->cond_, &l.m_.impl_->m_ );
	}
	while( res == EINTR );
	impl_->signalled_ = false;
}


void condition::wait( scoped_lock& l, duration const& timeout )
{
	if( impl_->signalled_ ) {
		impl_->signalled_ = false;
		return;
	}
	int res;
	do {
		timeval tv = {0, 0};
		gettimeofday( &tv, 0 );

		timespec ts;
		ts.tv_sec = tv.tv_sec + timeout.seconds();
		ts.tv_nsec = tv.tv_usec * 1000 + timeout.nanoseconds() % 1000000000;
		if( ts.tv_nsec > 1000000000ll ) {
			++ts.tv_sec;
			ts.tv_nsec -= 1000000000ll;
		}
		res = pthread_cond_timedwait( &impl_->cond_, &l.m_.impl_->m_, &ts );
	}
	while( res == EINTR );
	impl_->signalled_ = false;
}


void condition::signal( scoped_lock& )
{
	impl_->signalled_ = true;
	pthread_cond_signal( &impl_->cond_ );
}
