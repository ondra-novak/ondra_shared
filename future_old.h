#ifndef _ONDRA_SHARED_FUTURE_H_239087461782087
#define _ONDRA_SHARED_FUTURE_H_239087461782087

#include <atomic>
#include <cstddef>


#include "refcnt.h"
#include "waitableEvent.h"
#include "stringview.h"
#include "defer.h"


namespace ondra_shared {


template<typename FT> class FutureExceptionalState;
class FutureWaitableEvent;
///Generic variable contains a immutable value which is initialized in the future
/**
 * The instance of the FutureValue is immovable and immutable shared state between multiple threads.
 * The instance must be constructed either in stack or in heap, but cannot be  moved or transfered.
 *
 * The variable is either empty or initialized. Once it is initialized, it is immutable
 * (the value cannot be changed)
 *
 * Until the variable is initialized, other thread can synchronize with the variable.
 * Theads can also attach function blocks that are executed immediatelly after the
 * value is initialized.
 *
 * You can set value, wait synchronously and wait asynchronousy for the value. Synchronization
 * is removes any posibility of race conditions.
 *
 */
template<typename T>
class FutureValue {



public:



     ///Constructs uninitialized future variable
     FutureValue():watchChain(nullptr),rslv_lock(0) {}
     ///Destroys unitialized future variable
     /** If the variable is destroyed when in the empty (unresolved) state, the all handlers
      * are notified with exception UnresolvedFutureExcepton
      */
     ~FutureValue();


     ///Copies state of the other future
     /**
      * @param value other future. The state is copied only when source future
      * already resolved. Otherwise the unresolved future is constructed
      */
     FutureValue(const FutureValue &value):watchChain(nullptr),rslv_lock(0) {
          if (value.exception != nullptr) set(value.exception);
          else if (value.value) set(*value.value);
     }

     ///Copies state of the other future
     /**
      * @param value other future. The state is copied only when source future
      * already resolved. Otherwise the future is left unresolved. You cannot
      * assign to already resolved future
      */
     FutureValue &operator=(const FutureValue &value){
          if (&value != this && value.isResolved())
               set(value);
          return *this;
     }

     ///Retrieves value of the future
     /**
      * The function block until the future is resolved.
      * @return value of the future
      * @exception any the exception is thrown, if the future is rejected
      */
     const T &get() const;

     ///Equivalent to get()
     operator const T &() const;

     ///Sets value
     /**
      * @param value sets the value. You can set the value only once. All other
      * requests are ignored. This can be used for racing, while the first call
      * wins.
      */
     void set(const T &value);

     ///Sets value
     /**
      * @param value sets the value. You can set the value only once. All other
      * requests are ignored. This can be used for racing, while the first call
      * wins.
      */
     void set(T &&value);

     ///Sets the future into rejected state
     /**
      * @param exception specifies reason of rejection
      */
     void reject(const std::exception_ptr &exception);

     ///Sets the future into rejected state using current exception
     /**
      * You can only call this function inside of the exception handler (catch). Function
      * captures the exception and uses it to reject future
      *
      * If called outside of catch handler, RejectedFutureException is used
      */
     void reject();

     ///Resolves future using other future
     /**
      * This makes link with other future. Once the other future is resolved,
      * the value is used to resolve of this future.
      * @param other Specifies source future. If the source future is already resolved,
      * its value is used to resolve this future
      */
     void set(FutureValue &other);

     ///Blocks execution until the future is resolved
     void wait() const;

     ///Creates waitable event, which unblocks when future is resolved
     /** Waitable events implements waiting for specified timeout
      *
      * @note that each time the function is called, a new object is created. If you need
      * to repeatedly wait with a timeout just create one object and repeatedly wait on
      * it.
      *
      * @return new waitable event
      */
     std::shared_ptr<FutureWaitableEvent> create_waitable_event();

     ///Asynchronous waiting
     /**
      * Schedules function for execution after future is resolved
      * @param fn function to execute
      * @retval true the future is already resolved
      * @retval false the future is nor resolved, the function is scheduled
      *
      * @note function is not called when the future is resolved by the exception
      */
     template<typename Fn> bool await(Fn &&fn);

     ///Asynchronous waiting
     /**
      * Schedules function for execution after future is resolved
      * @param fn function to execute
      * @param exceptFn function to execute when future is rejected
      * @retval true the future is already resolved
      * @retval false the future is not resolved, the function is scheduled
      */
     template<typename Fn, typename ExceptFn> bool await(Fn &&fn, ExceptFn &&exceptFn);

     ///Asynchronous waiting
     /** Schedules function for exception after the future is resolved
      *
      * @param exceptFn function to execute when future is rejected
      * @retval true the future is already resolved
      * @retval false the future is not resolved, the function is schedled
      *
      * @note If the future is resolved with a valid result the function is not caled, it is only
      * removed
      */
     template<typename ExceptFn> bool await_catch(ExceptFn &&exceptFn);

     template<typename Fn> bool then(Fn &&fn);

     template<typename Fn, typename ExceptFn> bool then(Fn &&fn, ExceptFn &&exceptFn);

     template<typename ExceptFn> bool then_catch(ExceptFn &&exceptFn);

     template<typename Fn> void finally(Fn &&fn);

     template<typename Arg>     void operator << (Arg && arg);

     template<typename Fn> void operator >> (Fn && arg);

     FutureExceptionalState<FutureValue<T> > operator !() ;

     bool is_resolved() const;

     bool is_rejected() const;


protected:

     class AbstractWatchHandler;

     class AbstractWatchHandler {
     public:
          ///called when future is resolved with a value
          virtual void onValue(const T &value) noexcept(true) = 0;
          ///called when future is resolved with an exception
          virtual void onException(const std::exception_ptr &exception) noexcept(true) = 0;
          ///always called when handler is no longer needed
          /** You can use this function to cleanup the handler
           *
           * Other use of this function is to perform some general action regardless on
           * the value, because the function is always called even if the handler is not
           * added to the handler chain
           *
           */
          virtual void release() {delete this;}
          virtual ~AbstractWatchHandler() {}
          ///next handle in the chain, do not touch it.
          AbstractWatchHandler *next;
     };

     ///Adds abstract watch handler
     /**
      * @param h pointer to handler.
      * @retval true handler was not added, future is already resolved, however, the function release() is called
      * @retval false handler was added and waiting
      */
     bool addHandler(AbstractWatchHandler *h);


     //typedef std::unique_lock<std::mutex> Sync;

     std::atomic<AbstractWatchHandler *> watchChain;
     std::atomic<int> rslv_lock;
     const T *value = nullptr;
     std::exception_ptr exception;
     unsigned char valueBuffer[sizeof(T)];

     //std::mutex lock;

     template<typename Fn, typename ValueType>
     bool resolve(const Fn &fn, const ValueType &value, AbstractWatchHandler *skip=nullptr);
     bool autoresolve(AbstractWatchHandler *skip=nullptr);

     class WaitHandler;
     template<typename Fn> class WaitHandlerAwait;
     template<typename Fn1, typename Fn2> class WaitHandlerAwaitCatch;
     template<typename Fn> class WaitHandlerCatch;

     template<typename Fn> static auto callCb(Fn &&fn, const T &t) -> decltype(fn(t),void()) {
          defer >> [t=T(t),fn=Fn(fn)]() mutable {fn(t);};
     }
     template<typename Fn> static auto callCb(Fn &&fn, const T &) -> decltype(fn(),void()) {
          defer >> fn;
     }
     template<typename Fn> static auto callCbExcept(Fn &&fn, const std::exception_ptr &t) -> decltype(fn(t),void()) {
          defer >> [t=std::exception_ptr(t),fn=Fn(fn)]() mutable {fn(t);};
          fn(t);
     }
     template<typename Fn> static auto callCbExcept(Fn &&fn, const std::exception_ptr &) -> decltype(fn(),void()) {
          defer >> fn;
          fn();
     }

     bool lockResolve() {
          int expected = 0;
          return rslv_lock.compare_exchange_strong(expected,1);
     }
     bool commitResolve() {
          int expected = 1;
          return rslv_lock.compare_exchange_strong(expected,2);
     }


};
template<typename T> class Future;


template<typename X>
class FutureFromType;


///Future class - future variable which contains shared state of FutureValue
/**
 * The main difference from FutureValue is, that shared state is kept on heap and the
 * object just contains reference to it. The variable of the type Future can be shared or
 * returned from a function as result (this is not possible with FutureValue)
 */
template<typename T>
class Future {


public:
     typedef std::shared_ptr<FutureValue<T> > PFutureValue;

     template<typename Fn> using RetFnThen = typename FutureFromType< decltype(std::declval<Fn>()(std::declval<T>()))>::type;
     template<typename Fn> using RetFnCatch = typename FutureFromType< decltype(std::declval<Fn>()(std::declval<std::exception_ptr>()))>::type;
     template<typename Fn> using RetFnThenVoid = typename FutureFromType< decltype(std::declval<Fn>()())>::type;


     ///Create and initialize future variable (the shared state)
     Future();
     ///Create uninitialized future variable. It cannot be used until someone assign it a value
     Future(std::nullptr_t);
     ///Create already resolved future variable
     static Future resolve(const T &val);
     ///Create already resolved future variable
     static Future resolve(T &&val);
     ///Create a future which is initialized by other future
     static Future resolve(Future<T> val);
     ///Create a rejected future, which is rejected by current exception (call in catch handler)
     static Future rejected();
     ///Create a rejected future, which is rejected by specific exception
     static Future rejected(const std::exception_ptr &e);

     ///Get value
     /**
      * Function block when the value is not set.
      * @return value
      * @exception any if the future is rejected, the exception is thrown
      */
     const T &get() const;
     ///Get The value
     operator const T &() const;
     ///Set the value
     /**
      * @param value new value. You can set the value only once. Additional atempts are ignored
      */
     void set(const T &value);
     ///Set the value
     /**
      * @param value new value. You can set the value only once. Additional atempts are ignored
      */
     void set(T &&value);
     ///Sets the value base on other futurue
     /**
      * @param value an other future. Futures are linked, so the value can be set later when to source future
      * becomes resolved
      */
     void set(Future<T> value);

     ///Cals specified function and uses result to resolve the future
     /**
      * @param fn function to call
      * @param args arguments
      */
     template<typename Fn, typename ... Args>
     void set_result_of(Fn &&fn, Args && ... args);


     ///Reject the future with an exception
     /**
      * @param exception exception
      */
     void reject(const std::exception_ptr &exception);
     ///Reject the future with current exception
     void reject();
     ///Wait for resolution
     /** Function blocks until the future is set
      */
     void wait() const;

     std::shared_ptr<FutureWaitableEvent> create_waitable_event();

     ///Asynchronous waiting
     /**
      * Performs waiting asynchronously. Suply the code which is executed once the value is set.
      *
      * @param fn function to execite
      * @retval true future is already resolved, so code will not be executed
      * @retval false future is not resolved, the code will be executed
      */
     template<typename Fn> bool await(Fn &&fn);
     ///Asynchronous waiting
     /**
      * Performs waiting asynchronously. Suply the code which is executed once the value is set.
      *
      * @param fn function to execute
      * @param exceptFn function to execute when the value is rejected
      * @retval true future is already resolved, so code will not be executed
      * @retval false future is not resolved, the code will be executed
      */
     template<typename Fn, typename ExceptFn> bool await(Fn &&fn, ExceptFn &&exceptFn);
     ///Asynchronous waiting
     /**
      * Performs waiting asynchronously. Suply the code which is executed once the value is set.
      *
      * @param exceptFn function to execute when the value is rejected
      * @retval true future is already resolved, so code will not be executed
      * @retval false future is not resolved, the code will be executed
      *
      */
     template<typename ExceptFn> bool await_catch(ExceptFn &&exceptFn);
     ///Specifies function, which is called upon future resolution.
     /**
      *
      * @param fn function to call when future's value is set. The function must accept the future's value as argument.
      *  The function can return a value of another type or a Future value.
      * @return Future of return value. If the function throws exception, the returned future becomes rejected

      * @note rejected state of the source future is directly transfered to returning future

      */
     template<typename Fn> RetFnThen<Fn> then(Fn &&fn);
     ///Specifies function, which is called upon future resolution.
     /**
      *
      * @param fn function to call when future's value is set. The function is called without arguments
      *  The function can return a value of another type or a Future value.
      * @return Future of return value. If the function throws exception, the returned future becomes rejected

      * @note rejected state of the source future is directly transfered to returning future

      */
     template<typename Fn> RetFnThenVoid<Fn> then(Fn &&fn);
     ///Specifies function, which is called upon future resolution.
     /**
      *
      * @param fn function to call when future's value is set. The function must accept the future's value as argument.
      *  The function can return a value of another type or a Future value.
      * @param excepttFn function to call when future is in rejected state. The function must accept exception_ptr.
      *  The function must return a value of the same type as the fn().
      * @return Future of return value. If the function throws exception, the returned future becomes rejected


      */
     template<typename Fn, typename ExceptFn> RetFnThen<Fn> then(Fn &&fn, ExceptFn &&exceptFn);
     ///Specifies function, which is called upon future resolution.
     /**
      *
      * @param fn function to call when future's value is set. The function is called without arguments
      *  The function can return a value of another type or a Future value.
      * @param excepttFn function to call when future is in rejected state. The function must accept exception_ptr.
      *  The function must return a value of the same type as the fn().
      * @return Future of return value. If the function throws exception, the returned future becomes rejected


      */
     template<typename Fn, typename ExceptFn> RetFnThenVoid<Fn> then(Fn &&fn, ExceptFn &&exceptFn);
     ///Specifies function, which is called upon the future rejection
     /**
      * @param excepttFn function to call when future is in rejected state. The function must accept exception_ptr.
      *  The function must return a value of the same type as the type of the future
      * @return Future of return value. If the function throws exception, the returned future becomes rejected
      *
      * @note if the future is not in rejected state, the value is transfered to the returning future
      */


     template<typename ExceptFn> Future then_catch(ExceptFn &&exceptFn);

     template<typename Fn> Future finally( Fn &&fn);


     ///Alternative way to set value
     template<typename Arg>     void operator << (Arg && arg);

     ///Alternative way to call then()
     template<typename Fn> auto operator >> (Fn && fn) -> decltype(this->then(fn));

     ///Controls exceptional state of the future variable
     /**
      * @code
      * !(fn() >> [=](int x) {... normal behaviour...; return x;}) >> [=](std::exception_ptr &e) {...exception behaviour; return 42;};
      * @endcode
      */
     FutureExceptionalState<Future<T> > operator !() ;

     ///Returns true if future is resolved (set or rejected)
     bool is_resolved() const;
     ///Returns true if future is rejected
     bool is_rejected() const;


     template<typename Iteratable>
     static Future<std::vector<T> > all(Iteratable &&flist);
     static Future<std::vector<T> > all(std::initializer_list<Future<T> > &&flist);

     template<typename Iteratable>
     static Future race(Iteratable &&flist);
     static Future race(std::initializer_list<Future<T> > &&flist);


protected:

     PFutureValue v;
};

template<typename R> class FutureFromType {
public: typedef Future<typename std::remove_reference<R>::type> type;
};
template<typename R> class FutureFromType< Future<R> > {
public: typedef Future<R> type;
};
struct FutureEmptyValue{};
template<> class FutureFromType< Future<FutureEmptyValue> > {
public: typedef Future<void> type;
};


template<>
class Future<void>: public Future<FutureEmptyValue> {
public:
     Future() {}
     Future(const Future<FutureEmptyValue> &e):Future<FutureEmptyValue>(e) {}
     using Future<FutureEmptyValue>::Future;

     static Future resolve() {
          Future<void> f;
          f.set();
          return f;
     }

     void set() {
          Future<FutureEmptyValue>::set(FutureEmptyValue());
     }
     void set(const Future<void> &f) {
          Future<FutureEmptyValue>::set(f);
     }
     template<typename Fn, typename ... Args>
     void set_result_of(Fn &&fn, Args && ... args);
protected:
     template<typename Fn, typename ... Args>
     void set_result_of_select(std::true_type, Fn &&fn, Args && ... args);
     template<typename Fn, typename ... Args>
     void set_result_of_select(std::false_type, Fn &&fn, Args && ... args);
};


class UnresolvedFutureExcepton: public std::exception {
public:
     virtual const char *what() const noexcept(true) {return "Unresolved future";}
};

class RejectedFutureExcepton: public std::exception {
public:
     virtual const char *what() const noexcept(true) {return "Future rejected with no reason";}
};

template<typename T>
template<typename Fn, typename ValueType>
bool FutureValue<T>::resolve(const Fn &fn, const ValueType &value, AbstractWatchHandler *skip) {
     //walks through handlers and executes them with the value
     //it can also skip one handler, if specified by an argument
     bool skipped = false;
     //we fetch list of handlers by atomic exchange
     //this also sets waitChain to null and prevents conflicts during the resolve procedure

     //note there is no remove function so we can do this with no expected problems

     AbstractWatchHandler *chain = watchChain.exchange(nullptr);


     //walk the chain
     while (chain != nullptr) {
          AbstractWatchHandler *z = chain;
          chain = z->next;
          //skip handler if equal
          if (z != skip) {

               (z->*fn)(value);
               z->release();
          } else {
               //report skipped state if skipped
               skipped = true;
          }
     }
     //return skipped state (true = skipped, not called)
     return skipped;
}

template<typename T> bool FutureValue<T>::autoresolve(AbstractWatchHandler *skip) {
     //this is called when added handler need to ensure, that was not added right after resolution
     if (is_resolved()) {
          //so depend on how future is resolved, call apropriate resolve function
          if (value) {
               return resolve(&AbstractWatchHandler::onValue, *value, skip);
          } else if (exception) {
               return resolve(&AbstractWatchHandler::onException, exception, skip);
          } else {
               return true;
          }
     } else {
          //future is not resolved, return false
          return false;
     }
}

template<typename T>
void FutureValue<T>::set(const T &v) {
     if (!lockResolve()) return;

     value = new (valueBuffer) T(v);
     if (commitResolve()) {
          resolve(&AbstractWatchHandler::onValue, *value);
     }
}
template<typename T>
void FutureValue<T>::set(T &&v) {
     if (!lockResolve()) return;

     value = new (valueBuffer) T(std::move(v));
     if (commitResolve()) {
          resolve(&AbstractWatchHandler::onValue, *value);
     }

}

template<typename T>
inline FutureValue<T>::~FutureValue() {
     lockResolve();
     if (watchChain.load(std::memory_order_acquire) != nullptr) {
          resolve(&AbstractWatchHandler::onException, std::make_exception_ptr(UnresolvedFutureExcepton()));
     }
     if (value) value->~T();
}

template<typename T>
inline void FutureValue<T>::set(FutureValue<T>& other) {
     other.then([=](const T &val){
               set(val);
          },[=](const std::exception_ptr &exp){
               set(exp);
          });
}

template<typename T>
inline bool FutureValue<T>::is_resolved() const {
     return rslv_lock.load(std::memory_order_acquire) == 2;
}

template<typename T>
inline const T& FutureValue<T>::get() const {
     wait();
     if (exception) std::rethrow_exception(exception);
     return *value;
}

template<typename T>
inline FutureValue<T>::operator const T&() const {
     return get();
}

template<typename T>
inline bool FutureValue<T>::is_rejected() const {
     return is_resolved() && exception != nullptr;
}


template<typename T>
void FutureValue<T>::reject(const std::exception_ptr &exception) {
     if (!lockResolve()) return;

     this->exception =exception;
     if (commitResolve()) {
          resolve(&AbstractWatchHandler::onException, exception);
     }
}

template<typename T>
void FutureValue<T>::reject() {
     std::exception_ptr ptr = std::current_exception();
     if (ptr == nullptr) {
          ptr = std::make_exception_ptr(RejectedFutureExcepton());
     }
     reject(ptr);
}


template<typename T>
class FutureValue<T>::WaitHandler: public FutureValue<T>::AbstractWatchHandler {
public:
     virtual void onValue(const T &) noexcept(true) {
          //nothing there, actually we trigger on release()
     }
     virtual void onException(const std::exception_ptr &) noexcept(true) {
          //nothing there, actually we trigger on release()
     }
     virtual void release() {
          //release is called when handler is removed from the chain
          //this is our chance to notify waiting thread
          std::unique_lock<std::mutex> _(lock);
          fired = true;
          condVar.notify_all();
     }

     std::condition_variable condVar;
     std::mutex lock;
     bool fired = false;
};

template<typename T>
inline void FutureValue<T>::wait() const {

     //no wait if resolved
     if (is_resolved()) return;
     //wait handler (statically allocated)
     WaitHandler wh;
     //add handler, if true returned - already resolved
     if (const_cast<FutureValue<T> *>(this)->addHandler(&wh)) return;
     //yeild any defered function (to prevent deadlock)
     defer_yield();
     //acquire the lock
     std::unique_lock<std::mutex> mx(wh.lock);
     //wait for being fired.
     wh.condVar.wait(mx,[&]{return wh.fired;});
}



template<typename T>
inline bool FutureValue<T>::addHandler(AbstractWatchHandler *h) {

     //there must be deterministic way to detect, whether the handler would be called
     //in current thread or in different thread.

     //calling handler in current thread is forbidden, function must reject the handler
     //and return true as "already resolved" (see await() )

     //the handler is still created and function is copied into it, but will not be called
     //on already resolved state

     //because we are operating in lockfree environment, the possible race condition is
     //solved by making varions checks in sequential order

     AbstractWatchHandler *expected;
     do {
          //initialize expected variable, variable contains expected top handler
          expected = watchChain.load(std::memory_order_acquire);
          //initialize next pointer
          h->next = expected;
          //try to exchange top handler with new handler (lockfree push)
          //repeate cycle if fails - other addHandler is called, or future is being resolved now
     } while (!watchChain.compare_exchange_weak(expected, h));

     //we added handler to handler chain, but if the future is already resolved
     //the handler leaks

     //autoresolve resolves all handler in chain when future is already resolved
     bool b = autoresolve(h);
     //b = true indicates that handler was not called yet
     //if so, release it now
     if (b) h->release();
     //returns true, as "already resolved", false otherwise (handler was called by other thread)
     return b;
}

class FutureWaitableEvent: public WaitableEvent {
public:
     using WaitableEvent::WaitableEvent;

     bool wait(unsigned int timeout_ms) {defer_yield();return c.wait(timeout_ms);}
     void wait() {defer_yield();c.wait();}
     template<typename TimePoint> bool wait_until(const TimePoint &tp) {defer_yield();return c.wait_until(tp);}
     template<typename Duration> bool wait_for(const Duration &dur) {defer_yield();return c.wait_for(dur);}

};

template<typename T>
inline std::shared_ptr<FutureWaitableEvent> FutureValue<T>::create_waitable_event()  {
     std::shared_ptr<FutureWaitableEvent> ev (std::make_shared<FutureWaitableEvent>(false));

     auto fn = [ev] {
          ev->signal();
     };

     finally(fn);

     return ev;


}


template<typename T>
template<typename Fn>
class FutureValue<T>::WaitHandlerAwait: public FutureValue<T>::AbstractWatchHandler {
public:
     WaitHandlerAwait(Fn &&fn):fn(std::forward<Fn>(fn)) {}
     virtual void onValue(const T &value) noexcept(true) {callCb(fn,value);}
     virtual void onException(const std::exception_ptr &) noexcept(true) {}
private:
     std::remove_reference_t<Fn> fn;
};

template<typename T>
template<typename Fn>
class FutureValue<T>::WaitHandlerCatch: public FutureValue<T>::AbstractWatchHandler {
public:
     WaitHandlerCatch(Fn &&fn):fn(std::forward<Fn>(fn)) {}
     virtual void onValue(const T &) noexcept(true) {}
     virtual void onException(const std::exception_ptr &exception) noexcept(true) {callCbExcept(fn,exception);}
private:
     std::remove_reference_t<Fn> fn;
};

template<typename T>
template<typename Fn1,typename Fn2>
class FutureValue<T>::WaitHandlerAwaitCatch: public FutureValue<T>::AbstractWatchHandler {
public:
     WaitHandlerAwaitCatch(Fn1 &&fn1,Fn2 &&fn2):fn1(std::forward<Fn1>(fn1)),fn2(std::forward<Fn2>(fn2)) {}
     virtual void onValue(const T &value) noexcept(true) {callCb(fn1,value);}
     virtual void onException(const std::exception_ptr &exception) noexcept(true) {callCbExcept(fn2,exception);}
private:
     std::remove_reference_t<Fn1> fn1;
     std::remove_reference_t<Fn2> fn2;
};


template<typename T>
template<typename Fn>
inline bool FutureValue<T>::await(Fn&& fn) {
     if (is_resolved()) return true;
     return addHandler(new WaitHandlerAwait<Fn >(std::forward<Fn>(fn)));
}

template<typename T>
template<typename Fn, typename ExceptFn>
inline bool FutureValue<T>::await( Fn&& fn, ExceptFn&& exceptFn) {
     if (is_resolved()) return true;
     return addHandler(new WaitHandlerAwaitCatch<Fn , ExceptFn >(std::forward<Fn>(fn),std::forward<ExceptFn>(exceptFn)));
}

template<typename T>
template<typename ExceptFn>
inline bool FutureValue<T>::await_catch(ExceptFn&& exceptFn) {
     if (is_resolved()) return true;
     return addHandler(new WaitHandlerCatch<ExceptFn >(std::forward<ExceptFn>(exceptFn)));
}

template<typename T>
template<typename Fn>
inline bool FutureValue<T>::then(Fn&& fn) {
     if (await(fn)) {
          if (!is_rejected()) {
               callCb(std::forward<Fn>(fn),*value);
          }
          return true;
     } else {
          return false;
     }
}

template<typename T>
template<typename Fn, typename ExceptFn>
inline bool FutureValue<T>::then(Fn&& fn, ExceptFn&& exceptFn) {
     if (await(fn, exceptFn)) {
          if (is_rejected()) callCbExcept(std::forward<ExceptFn>(exceptFn),exception);
          else callCb(std::forward<Fn>(fn),*value);;
          return true;
     } else {
          return false;
     }
}

template<typename T>
template<typename ExceptFn>
inline bool FutureValue<T>::then_catch(ExceptFn&& exceptFn) {
     if (await_catch(exceptFn)) {
          if (is_rejected()) callCbExcept(std::forward<ExceptFn>(exceptFn),exception);
          return true;
     } else {
          return false;
     }
}

template<typename FT> class FutureExceptionalState {
public:
     FutureExceptionalState(FT &owner):owner(owner) {}
     template<typename Fn> auto operator >> (Fn &&fn)
          -> typename FutureFromType<decltype(std::declval<Fn>()(std::declval<std::exception_ptr>()))>::type {
          return owner.then_catch(fn);
     }
     operator bool() const {return !owner.is_resolved();}
     void operator << (const std::exception_ptr &ptr) {
          owner.reject(ptr);
     }

     auto getFuture() const {return owner;}
protected:
     FT &owner;
};

template<typename T> template<typename Arg>
inline void FutureValue<T>::operator <<(Arg&& arg){set(std::forward<Arg>(arg));}
template<typename T> template<typename Fn>
inline void FutureValue<T>::operator >>(Fn&& fn){then(std::forward<Fn>(fn));}


template<typename T>
FutureExceptionalState<Future<T> > Future<T>::operator !() {
     return FutureExceptionalState<Future<T> > (*this);
}

template<typename T> Future<T>::Future() {
     v = std::make_shared<FutureValue<T> >();
}
template<typename T>
Future<T>::Future(std::nullptr_t) {}

template<typename T> Future<T> Future<T>::resolve(const T &val) {
     Future f;
     f.set(val);
     return f;
}
template<typename T> Future<T> Future<T>::resolve(T &&val) {
     Future f;
     f.set(std::move(val));
     return f;

}
template<typename T> Future<T> Future<T>::resolve(Future<T> val) {
     Future f;
     f.set(val);
     return f;
}
template<typename T> Future<T> Future<T>::rejected() {
     Future f;
     f.reject();
     return f;

}
template<typename T> Future<T> Future<T>::rejected(const std::exception_ptr &e) {
     Future f;
     f.reject(e);
     return f;
}


template<typename T> const T &Future<T>::get() const {
     return v->get();
}
template<typename T> Future<T>::operator const T &() const {
     return v->get();
}
template<typename T> void Future<T>::set(const T &value) {
     v->set(value);
}
template<typename T> void Future<T>::set(T &&value) {
     v->set(std::move(value));
}
template<typename T> void Future<T>::set(Future<T> value) {
     Future<T> me(*this);
     value.v->then([=](const T &v) mutable {me.set(v);}
                 ,[=](const std::exception_ptr &v) mutable {me.reject(v);});
}
template<typename T> void Future<T>::reject(const std::exception_ptr &exception) {
     v->reject(exception);
}
template<typename T> void Future<T>::reject() {
     v->reject();
}
template<typename T> void Future<T>::wait() const {
     v->wait();
}

template<typename T> inline std::shared_ptr<FutureWaitableEvent> Future<T>::create_waitable_event() {
     return v->create_waitable_event();
}


template<typename T> template<typename Fn> bool Future<T>::await(Fn &&fn) {
     return v->await(std::forward<Fn>(fn));
}
template<typename T> template<typename Fn, typename ExceptFn> bool Future<T>::await(Fn &&fn, ExceptFn &&exceptFn) {
     return v->await(std::forward<Fn>(fn), std::forward<ExceptFn>(exceptFn));
}
template<typename T> template<typename ExceptFn> bool Future<T>::await_catch(ExceptFn &&exceptFn) {
     return v->await_catch(std::forward<ExceptFn>(exceptFn));
}
template<typename T> template<typename Fn> typename Future<T>::template RetFnThen<Fn> Future<T>::then(Fn &&fn) {
     typename Future<T>::template RetFnThen<Fn> f;
     v->then([f,fn=Fn(std::forward<Fn>(fn))](const T &val) mutable {
          f.set_result_of(fn, val);
     },[f](const std::exception_ptr &e) mutable  {
          f.reject(e);
     });
     return f;
}
template<typename T> template<typename Fn, typename ExceptFn> typename Future<T>::template RetFnThen<Fn> Future<T>::then(Fn &&fn, ExceptFn &&exceptFn) {
     typename Future<T>::template RetFnThen<Fn>  f;
     v->then([f,fn=Fn(std::forward<Fn>(fn))](const T &val) mutable {
          f.set_result_of(fn, val);
     },[f,efn=ExceptFn(std::forward<ExceptFn>(exceptFn))](const std::exception_ptr &e) mutable  {
          try {
               f.set(efn(e));
          } catch (...) {
               f.reject();
          }
     });
     return f;
}
template<typename T> template<typename Fn> typename Future<T>::template RetFnThenVoid<Fn> Future<T>::then(Fn &&fn) {
     typename Future<T>::template RetFnThenVoid<Fn> f;
     v->then([f,fn=Fn(std::forward<Fn>(fn))](const T &) mutable {
          f.set_result_of(fn);
     },[f](const std::exception_ptr &e) mutable  {
          f.reject(e);
     });
     return f;
}
template<typename T> template<typename Fn, typename ExceptFn> typename Future<T>::template RetFnThenVoid<Fn> Future<T>::then(Fn &&fn, ExceptFn &&exceptFn) {
     typename Future<T>::template RetFnThenVoid<Fn>  f;
     v->then([f,fn=Fn(std::forward<Fn>(fn))](const T &) mutable {
          f.set_result_of(fn);
     },[f,efn=ExceptFn(std::forward<ExceptFn>(exceptFn))](const std::exception_ptr &e) mutable {
          try {
               f.set(efn(e));
          } catch (...) {
               f.reject();
          }
     });
     return f;
}
template<typename T> template<typename ExceptFn> Future<T> Future<T>::then_catch(ExceptFn &&exceptFn) {
     return then([=](const T &x) {return x;},std::forward<ExceptFn>(exceptFn));
}
template<typename T> template<typename Arg>     void Future<T>::operator << (Arg && arg) {
     set(std::forward<Arg>(arg));
}
template<typename T> template<typename Fn> auto Future<T>::operator >> (Fn && fn) -> decltype(this->then(fn)) {
     return then(fn);
}

template<typename T> template<typename Fn> void FutureValue<T>::finally(Fn &&fn) {

     class Spec: public AbstractWatchHandler {
     public:
          Spec(Fn &&fn):fn(fn) {}
          virtual void onValue(const T &) noexcept(true) override {fn();}
          virtual void onException(const std::exception_ptr &) noexcept(true) override  {fn();}

          std::remove_reference_t<Fn> fn;
     };

     if (is_resolved()) {
          defer >> fn;
     }
     else {
          if (addHandler(new Spec(fn)))
               defer >> fn;
     }

}

template<typename T> template<typename Fn> Future<T> Future<T>::finally(Fn &&fn) {
     v->finally(fn);
     return *this;
}
template<typename T> bool Future<T>::is_resolved() const {
     return v->is_resolved();
}
template<typename T> bool Future<T>::is_rejected() const {
     return v->is_rejected();
}

namespace _details {


     ///Function which tracks resolution of all futures
     /**
      * Counts how many futures is resolved. Once all futures are resolved,
      * it picks results and resolves future containing vector of results
      */
     template<typename T>
     class AllFutureHelper {
     public:

          class SharedState: public RefCntObj{
          public:
               Future<std::vector<T> >result;
               std::vector<Future<T> > flist;
               std::atomic<unsigned int> remain;

               SharedState():result(nullptr),remain(0) {}
          };

          RefCntPtr<SharedState> state;

          AllFutureHelper(const Future<std::vector<T> >&result):state(new SharedState()) {
               state->result = result;
          }

          void add(Future<T> f) {
               ++state->remain;
               state->flist.push_back(f);
               f.finally(*this);
          }


          void operator()() {
               if (--state->remain == 0) {
                    std::vector<T> resultArr;
                    for (auto &&x: state->flist)
                         resultArr.push_back(x.get());
                    state->result << std::move(resultArr);
               }
          }
     };

template<typename T,typename Iteratable>
Future<std::vector<T> > future_all_impl(Iteratable &&flist) {

     Future<std::vector<T> > res;
     AllFutureHelper<T> hlp(res);
     for (auto &&x : flist) {
          hlp.add(x);
     }
     return res;
}

template<typename T, typename Iteratable>
Future<T> future_race_impl(Iteratable &&flist) {
     //Race command is that easy.
     //Because resolving already resolved future is not an error
     //we can connect the target future with many sources. First resulve future resolves
     //the target future, other will be ignored
     Future<T> out;
     for (auto &&x : flist) {
          out << x;
     }
     return out;
}

}

template<typename T>
template<typename Iteratable>
Future<std::vector<T> > Future<T>::all(Iteratable &&flist) {
     return _details::future_all_impl(std::forward<Iteratable>(flist));
}

template<typename T>
Future<std::vector<T> > Future<T>::all(std::initializer_list<Future<T> > &&flist) {
     return _details::future_all_impl<T,std::initializer_list<Future<T> > >(std::forward<std::initializer_list<Future<T> > >(flist));
}


template<typename T>
template<typename Iteratable>
Future<T> Future<T>::race(Iteratable &&flist) {
     return _details::future_race_impl(std::forward<Iteratable>(flist));
}

template<typename T>
Future<T> Future<T>::race(std::initializer_list<Future<T> > &&flist) {
     return _details::future_race_impl<T, std::initializer_list<Future<T> > >(std::forward<std::initializer_list<Future<T> > >(flist));
}

template<typename T>
template<typename Fn, typename ... Args>
inline void Future<T>::set_result_of(Fn &&fn, Args && ... args) {
     try {
          set(T(fn(std::forward<Args>(args)...)));
     } catch (...) {
          reject();
     }
}


template<typename Fn, typename ... Args>
inline void Future<void>::set_result_of(Fn &&fn, Args && ... args) {
     set_result_of_select(
               typename std::is_void<decltype(fn(std::forward<Args>(args)...))>::type(),
               std::forward<Fn>(fn),
               std::forward<Args>(args)...);
}
template<typename Fn, typename ... Args>
inline void Future<void>::set_result_of_select(std::true_type,Fn &&fn, Args && ... args) {
     try {
          fn(std::forward<Args>(args)...);set();
     } catch (...) {
          reject();
     }
}
template<typename Fn, typename ... Args>
inline void Future<void>::set_result_of_select(std::false_type,Fn &&fn, Args && ... args) {
     try {
          set(fn(std::forward<Args>(args)...));
     } catch (...) {
          reject();
     }
}


template<typename Fn>
class FutureCall: public FutureFromType<decltype(std::declval<Fn>()())>::type {
public:

     typedef typename FutureFromType<decltype(std::declval<Fn>()())>::type Future;
     FutureCall(Fn &&fn):fn(std::forward<Fn>(fn)) {}

     Future operator()() const {
          Future(*this).set_result_of(std::forward<Fn>(fn));
          return *this;
     }
     template<typename X>
     Future operator >> (X && x) const {
          x >> [me=Future(*this),fn = this->fn]() mutable {
               me.set_result_of(fn);
          };
          return *this;
     }

protected:
     mutable std::remove_reference_t<Fn> fn;
};

enum FutureCallKW {
     future_call
};

template<typename Fn>
auto operator>> (FutureCallKW, Fn &&fn) {
     return FutureCall<Fn>(std::forward<Fn>(fn));
}



};

#endif
