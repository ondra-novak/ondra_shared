/*
 * semaphore.h
 *
 *  Created on: 25. 6. 2018
 *      Author: ondra
 */

#ifndef __ONDRA_SHARED_SEMAPHORE_H25787_
#define __ONDRA_SHARED_SEMAPHORE_H25787_

#include <mutex>
#include  <condition_variable>


namespace ondra_shared {


class Semaphore{
public:

	Semaphore(unsigned int count):counter(count) {}


	bool wait(unsigned int timeout_ms) {
		std::unique_lock<std::mutex> _(mtx);
		if (counter == 0) {
			if (!waiter.wait_for(_,std::chrono::milliseconds(timeout_ms), [=]{return counter > 0;})) {
				return false;
			}
		}
		--counter;
		return true;
	}

	void wait() {
		std::unique_lock<std::mutex> _(mtx);
		waiter.wait(_,[=]{return counter > 0;});
		--counter;
	}

	template<typename TimePoint>
	bool wait_until(const TimePoint &tp) {
		std::unique_lock<std::mutex> _(mtx);
		if (counter == 0) {
			if (!waiter.wait_until(_,tp, [=]{return counter > 0;})) {
				return false;
			}
		}
		--counter;
		return true;
	}

	template<typename Duration>
	bool wait_for(const Duration &dur) {
		std::unique_lock<std::mutex> _(mtx);
		if (counter == 0) {
			if (!waiter.wait_for(_,dur, [=]{return counter > 0;})) {
				return false;
			}
		}
		--counter;
		return true;
	}

	Semaphore &operator++() {
		std::unique_lock<std::mutex> _(mtx);
		if (counter == 0) {
			waiter.notify_one();
		}
		++counter;
		return *this;
	}

	Semaphore &operator--() {
		wait();
		return *this;
	}

	void lock() {
		operator--();
	}

	void unlock() {
		operator++();
	}

	void signal() {
		operator++();
	}

protected:
	std::mutex mtx;
	std::condition_variable waiter;
	unsigned int counter ;

};

};




#endif /* __ONDRA_SHARED_SEMAPHORE_H25787_ */
