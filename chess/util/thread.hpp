#ifndef __THREAD_HPP__
#define __THREAD_HPP__

class thread {
	class impl;
public:
	thread();
	virtual ~thread();

	void spawn();
	void join();

	bool spawned() const;

	virtual void onRun() = 0;

private:
	impl* impl_;
};

#endif
