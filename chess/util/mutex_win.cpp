#include "mutex.hpp"
#include "windows.hpp"

class mutex::impl
{
public:
	impl()
	{
		InitializeCriticalSection(&cs_);
	}

	~impl()
	{
		DeleteCriticalSection(&cs_);
	}

public:
	CRITICAL_SECTION cs_;
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
	EnterCriticalSection( &m.impl_->cs_ );
	locked_ = true;
}


scoped_lock::~scoped_lock()
{
	if( locked_ ) {
		LeaveCriticalSection( &m_.impl_->cs_ );
	}
}


void scoped_lock::lock()
{
	EnterCriticalSection( &m_.impl_->cs_ );
	locked_ = true;
}


void scoped_lock::unlock()
{
	LeaveCriticalSection( &m_.impl_->cs_ );
	locked_ = false;
}


class condition::impl
{
public:
	impl()
		: signalled_()
	{
		InitializeConditionVariable( &cond_ );
	}

	~impl()
	{
	}

	CONDITION_VARIABLE cond_;
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
	SleepConditionVariableCS( &impl_->cond_, &l.m_.impl_->cs_, INFINITE );
	impl_->signalled_ = false;
}


void condition::wait( scoped_lock& l, duration const& timeout )
{
	if( impl_->signalled_ ) {
		impl_->signalled_ = false;
		return;
	}
	SleepConditionVariableCS( &impl_->cond_, &l.m_.impl_->cs_, static_cast<DWORD>(timeout.milliseconds()) );
	impl_->signalled_ = false;
}


void condition::signal( scoped_lock& )
{
	impl_->signalled_ = true;
	WakeConditionVariable( &impl_->cond_ );
}

