#include "thread.hpp"
#include "platform.hpp"

#if WINDOWS
namespace {
extern "C" {
static DWORD WINAPI run( void* p ) {
	reinterpret_cast<thread*>(p)->onRun();
	return 0;
}
}
}

class thread::impl
{
public:
	impl()
		: t_()
	{
	}

	~impl()
	{
		join();
	}

	void join()
	{
		if( !t_ ) {
			return;
		}

		WaitForSingleObject( t_, INFINITE );

		CloseHandle( t_ );
		t_ = 0;
	}

	bool spawned() const
	{
		return t_ != 0;
	}

	void spawn( thread* parent )
	{
		t_ = CreateThread( NULL, 0, &run, parent, 0, 0 );
	}

	HANDLE t_;
};

#else

namespace {
extern "C" {
static void* run( void* p ) {
	reinterpret_cast<thread*>(p)->onRun();
	return 0;
}
}
}

class thread::impl
{
public:
	impl()
		: t_()
	{
	}

	~impl()
	{
		join();
	}

	void spawn( thread* parent )
	{
		t_ = new pthread_t;
		pthread_create( t_, 0, &run, parent );
	}

	void join()
	{
		if( !t_ ) {
			return;
		}
		pthread_join( *t_, 0 );
		delete t_;
		t_ = 0;
	}

	bool spawned() const
	{
		return t_ != 0;
	}

	pthread_t* t_;
};

#endif

thread::thread()
	: impl_(new impl())
{
}


thread::~thread()
{
	delete impl_;
}


void thread::join()
{	
}


bool thread::spawned() const
{
	return impl_ && impl_->spawned();
}


void thread::spawn()
{
	join();

	impl_->spawn( this );
}
