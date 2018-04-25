#ifndef __ONDRA_SHARED_MTCOUNTER_421350489746514
#define __ONDRA_SHARED_MTCOUNTER_421350489746514

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace ondra_shared {

///Counts up and down, and allows to thread to wait for the zero
/**
 * One group of threads is able to increase and decrease the counter.
 * Other group of threads can be stopped until the zero is reached.
 *
 * Useful in destructor while waiting to complete all pending operations.
 * Counter is used to count pending operations. Once the counter
 * reaches the zero, destruction can continue
 */
class MTCounter {
public:

	///construct object with counter zero = threads can pass
	MTCounter():counter(0) {}
	///construct object with counter above zero = block threads
	MTCounter(unsigned int counter):counter(counter) {}

	///increment counter
	void inc() {
		++counter;
	}
	///decrement counter
	/** if counter reached zero, it releases waiting threads */
	void dec() {
		if (counter) {
			if (--counter == 0) {
				waiter.notify_all();
			}
		}
	}


	///Acquire the object
	/** function is equivalent to inc(). But this function is introduced
	 * to confort to concept BasicLockable. You can use lock_guard to track
	 * ownership
	 */
	void lock() {inc();}
	///Relese the object
	/** function is equivalent to dec(). But this function is introduced
	 * to confort to concept BasicLockable. You can use lock_guard to track
	 * ownership
	 */
	void unlock() {dec();}

	///Wait for release all references
	/**
	 * @param timeout_ms you can specify timeout
	 */
	bool wait(unsigned int timeout_ms) {
		std::unique_lock<std::mutex> _(mtx);
		return waiter.wait_for(_,std::chrono::milliseconds(timeout_ms), [=]{return counter == 0;});
	}

	///wait for release all references
	void wait() {
		std::unique_lock<std::mutex> _(mtx);
		waiter.wait(_,[=]{return counter == 0;});
	}

	///wait until all threads are released or until specified time which happens the first
	/**
	 * @param tp a timepoint in the future
	 * @retval true thread released
	 * @retval false timeout
	 */
	template<typename TimePoint>
	bool wait_until(const TimePoint &tp) {
		std::unique_lock<std::mutex> _(mtx);
		return waiter.wait_until(_,tp, [=]{return counter == 0;});
	}

	///wait until all threads are released or for specified duration which happens the first
	/**
	 * @param dur duration
	 * @retval true thread released
	 * @retval false timeout
	 */
	template<typename Duration>
	bool wait_for(const Duration &dur) {
		std::unique_lock<std::mutex> _(mtx);
		return waiter.wait_for(_,dur, [=]{return counter == 0;});
	}


	///Receive counter value
	/** useful for debugging purposes
	 *
	 */
	unsigned int getCounter() const {
		return counter;
	}

	///Sets counter
	/**
	 * @param counter new counter value
	 * if the counter is set to zero, it immediately releases all waiting threads
	 */
	void setCounter(unsigned int counter) {
		this->counter = counter;
		if (counter == 0) {
			waiter.notify_all();
		}
	}

	///Sets the counter when it contains expected value
	/**
	 * @param expected expected value
	 * @param desired desired value
	 * @retval true value set
	 * @retval false value was not set
	 *
	 * @note only uses compare_exchange_strong function to atomically initialize the counter
	 */
	bool setCounterWhen(unsigned int expected, unsigned int desired, std::memory_order m = std::memory_order_seq_cst) {
		bool r = this->counter.compare_exchange_strong(expected,desired,m);
		if (r && desired == 0) {
			waiter.notify_all();
		}
		return r;
	}

	///deprecated function for compatibility
	bool zeroWait(unsigned int timeout_ms) {return wait(timeout_ms);}
	///deprecated function for compatibility
	void zeroWait() {return wait();}

protected:
	std::mutex mtx;
	std::condition_variable waiter;
	std::atomic<unsigned int> counter ;
};


}
#endif
