#ifndef __ONDRA_SHARED_MTCOUNTER_421350489746514
#define __ONDRA_SHARED_MTCOUNTER_421350489746514

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace ondra_shared {

///Counts up and down, and allows to thread to wait for the zero or negative
/**
 * One group of threads is able to increase and decrease the counter.
 * Other group of threads can be stopped until the zero is reached.
 *
 * Useful in destructor while waiting to complete all pending operations.
 * Counter is used to count pending operations. Once the counter
 * reaches the zero, destruction can continue
 *
 *
 * @note Countdown object is opened while counter is zero or below and closed when counter is above zero.
 */
class Countdown {
public:

     ///construct object with counter zero = threads can pass
     Countdown():counter(0) {}
     ///construct object with counter above zero = block threads
     Countdown(int counter):counter(counter) {}

     ///increment counter
     int inc() {
          std::unique_lock<std::mutex> _(mtx);
          return ++counter;
     }
     ///decrement counter
     /** if counter reached zero, it releases waiting threads */
     int dec() {
          std::unique_lock<std::mutex> _(mtx);
          int v = --counter;
          if (v == 0) {
               waiter.notify_all();
          }
          return v;
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
          return waiter.wait_for(_,std::chrono::milliseconds(timeout_ms), [=]{return counter <= 0;});
     }

     ///wait for release all references
     void wait() {
          std::unique_lock<std::mutex> _(mtx);
          waiter.wait(_,[=]{return counter <= 0;});
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
          return waiter.wait_until(_,tp, [=]{return counter <= 0;});
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
          return waiter.wait_for(_,dur, [=]{return counter <= 0;});
     }


     ///Receive counter value
     /** useful for debugging purposes
      *
      */
     int getCounter() const {
          std::unique_lock<std::mutex> _(mtx);
          return counter;
     }

     ///Sets counter
     /**
      * @param counter new counter value
      * if the counter is set to zero, it immediately releases all waiting threads
      */
     void setCounter(int counter) {
          std::unique_lock<std::mutex> _(mtx);
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
      */
     bool setCounterWhen(int expected, int desired) {
          std::unique_lock<std::mutex> _(mtx);
          if (expected == this->counter) {
               if (desired == 0) {
                    waiter.notify_all();
               }
               return true;
          } else {
               return false;
          }
     }

     int operator++() {return inc();}
     int operator--() {return dec();}
     int operator++(int) {return inc() - 1;}
     int operator--(int) {return dec() + 1;}

protected:
     mutable std::mutex mtx;
     std::condition_variable waiter;
     int counter ;
};

///Helps to count of locks, can be included into clousure of asynchronous function
class CountdownGuard {
public:
     CountdownGuard(Countdown &owner):owner(owner) {
          this->owner.inc();
     }
     ~CountdownGuard() {
          this->owner.dec();
     }
     CountdownGuard(const CountdownGuard &other):owner(other.owner) {
          this->owner.inc();
     }
protected:
     Countdown &owner;
};



}
#endif
