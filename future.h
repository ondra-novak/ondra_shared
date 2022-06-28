/*
 * future.h
 *
 *  Created on: 3. 9. 2020
 *      Author: ondra
 */

#ifndef _ONDRA_SHARED_FUTURE_H_23908746178ew323e7
#define _ONDRA_SHARED_FUTURE_H_23908746178ew323e7

#include <optional>
#include <exception>
#include <atomic>
#include <cstdint>
#include "refcnt.h"

namespace ondra_shared {

template<typename T> class Future;

namespace _details {


     template<typename T>
     class FutureCBBuilder {
     public:
          using RetVal = Future<T>;
          template<typename Fn, typename ProcessFn>
          static RetVal build(Fn &&fn, ProcessFn &&pfn);
     };

     template<typename T>
     class FutureCBBuilder<Future<T> > {
     public:
          using RetVal = Future<T>;
          template<typename Fn, typename ProcessFn>
          static RetVal build(Fn &&fn, ProcessFn &&pfn);
     };

     template<>
     class FutureCBBuilder<void> {
     public:
          using RetVal = void;
          template<typename Fn, typename  ProcessFn>
          static RetVal build(Fn &&fn, ProcessFn &&pfn);
     };

     class FutureSemaphore {
     public:
          std::mutex mx;
          std::condition_variable cond;
     };


}




///The Future is variable which's value is resolved later
/** This variable is created empty, like the std::optional, and later it
 * can have assigned value. In contrast to the std::optional, once the value
 * is set, it cannot be reset. On other hand, you can register callback
 * function which is called, once the variable is set, or you can issue wait()
 * to block execution of the thread until the variable is set.
 *
 * If the value of the variable cannot be determined due some exception,
 * the variable can remember the exception and propagate it to the callback
 *
 * @note MT issues - The class is implemented lock-free unless wait() function
 * is used. It expects, that there is only one thread which eventually resolves
 * the Future. If multiple threads needs to resolve the Future, then use
 * proper synchronization (mutex) to access to future's functions resolve() or
 * reject(). Note that the Future can be resolved only once. In other hand,
 * registering callbacks or retrieveing the state of the future is MT safe,
 * the multiple threads can access the future without need of synchronization.
 *
 * @tparam T type of the value. You can't use 'void'. If you need such
 * future, construct an empty class for example.
 */
template<typename T>
class Future;

///Used when future is resolved in callbacks. Just extends Future object by some extra information
/**
 * Primarily used to identify the context in which the callback is called. The
 * object has one constant boolean variable, which is initialized depend on whether
 * the callback is called in context resolving thread or registering thread.
 *
 * @see FutureResolved::nested
 */
template<typename T>
class FutureResolved;

template<typename T>
class Future {
public:

     ///construct empty. unresolved future
     Future();
     ///Create already resolved future
     /**
      * @param value value to resolve
      */
     Future(const T &value);
     ///Create already resolved future
     /**
      * @param value value to resolve
      */
     Future(T &&value);
     ///Create already resolved future with exception
     /**
      * @param exp exception object
      */
     Future(const std::exception_ptr &exp);

     enum Undefined {undefined};

     ///Construct undefined future
     /**
      * Undefined future is marked as undefined. It means, that value of future is not
      * defined and will not be defined in future. You can test this state by calling
      * defined() function. Note that undefined future cannot be used otherwise then
      * calling that function. Because its state is undefined. Accessing undefined future
      * can cause null pointer dereference
      *
      */
     Future(Undefined);

     ///Determines whether future is defined
     /** By default, future is defined unless it is constructed with undefined state. */
     bool defined() const;

     ///Attach the callback function to the future
     /**
      * Function is designed to accept a lamba function, but any function
      * can be used. The function is called when the future is resolved. If the
      * future is resolved already resolved, the function is immediatelly called.
      * It can be called in context of caller thread, so the caller can be
      * blocked until the function finishes. However it can be also called
      * in context of resolver's thread, especially if this happen, when
      * the resolving thread is executing callbacks registered before (right now).
      *
      * You cannot rely on context of which thread is function executed.
      *
      * @param fn function to execute when future is resolved
      * @return if the function has return value, the result is Future of the
      * returned type. This allows to chain additional callback to process
      * return value of the prevous callback. If the return value of the function
      * is void, then this operator also returns void
      *
      * @note the callback function accepts one argument - the future itself.
      * You can use get() function to read value.
      */
     template<typename Fn>
     auto operator>>(Fn &&fn) -> typename _details::FutureCBBuilder<decltype(fn(std::declval<Future>()))>::RetValue const {
          using RetVal = decltype(fn(std::declval<Future>()));
          return _details::FutureCBBuilder<RetVal>::build(std::forward<Fn>(fn), [&](auto &&fn){
               addCallback(fn);
          });
     }
     ///returns value of the future
     /** returns value. If the future is not resolved, then blocks. If the
      * future was resolved with exception, then the exception is thrown
      */
     const T &get() const;
     ///Resolves the future with value
     /** @note function is not MT safe
      *
      * @param val value to set
      */
     template<typename X>
     void resolve(X &&val) const;


     void resolve(T &&val) const;
     ///Rejects the future with an exception
     void reject(const std::exception_ptr &exp) const;
     ///Waits for resolution
     void wait() const;
     ///Waits for resolution
     template<typename A,typename B>
     bool wait_for(const std::chrono::duration<A,B> &period) const;
     ///Waits for resolution
     template<typename A,typename B>
     bool wait_until(const std::chrono::time_point<A,B> &t) const;
     ///Returns true, when future is already resolved
     bool resolved() const;

     class Callback {
     public:
          virtual void call(const FutureResolved<T> &fut) noexcept = 0;
          virtual ~Callback() {};
          Callback *next = nullptr;
     };



protected:

     using Semaphore = _details::FutureSemaphore;

     class State: public RefCntObj {
     public:
          //contains value when future is resolved
          std::optional<T> value;
          //contains exception when future is rejected
          std::exception_ptr exception;
          //contains true, when future is resolved
          /* Note, the content of value/exception is not defined, if the value is false.
           * Once the future is resolved, this variable is set to true
           */
          volatile std::atomic_bool resolved = false;
          //Contains all registered callbacks
          volatile std::atomic<Callback *> callbacks = nullptr;
          //Contains pointer to a semaphore, which controls waiting threads
          /*This variable is set to nullptr during initialization. The first incomming thread
           * creates the semaphore instance and sets the pointer (atomically), and starts
           * to waiting on it. Other threads can wait as well. The semaphore is opened
           * when future is resolved
           */
          volatile std::atomic<Semaphore *> semaphore = nullptr;
          ~State();
     };


     using PState = ondra_shared::RefCntPtr<State>;

     PState state;
     template<typename Fn>
     void addCallback(Fn &fn) const;
     void flushCallbacks(const Callback *cb) const;
     Semaphore *installSemaphore() const;
     void uninstallSemaphore() const;
};


template<typename T>
class FutureResolved: public Future<T> {
public:
     FutureResolved(const Future<T> &f, bool nested);

     ///Contains information about the context
     /**
      * If this variable is false, then callback is called in context of resolving thread.
      * So the thread which made registration could already continue in his work, and
      * the callback is detached from its stack.
      *
      * If this variable is true, then callback is called in context of registration thread.
      * This happen, when the registration is made on already resolved future. The registration
      * thread is blocked until the callback is finished.
      *
      */
     const bool nested;
};

template<typename T>
Future<T>::Future(const T &value):state (new State) {
     resolve(value);
}


template<typename T>
Future<T>::Future():state (new State) {
}

template<typename T>
Future<T>::Future(const std::exception_ptr &exp):state(new State) {
     reject(exp);
}

template<typename T>
Future<T>::Future(Undefined):state (nullptr) {
}


template<typename T>
bool Future<T>::resolved() const {
     return state->resolved.load();
}

namespace _details {

     template<typename T>
     class ResolveBy {
     public:
          template<typename X>
          static void resolve(const Future<X> &fut, const T &val) {
               fut.resolve(static_cast<X>(val));
          }
     };

     template<typename T>
     class ResolveBy<Future<T> > {
     public:
          template<typename X>
          static void resolve(const Future<X> &fut, const Future<T> &val) {
               val >> [f = Future<X>(fut)](const Future<T> &v) {
                    f.resolve(static_cast<X>(v.get()));
               };
          }
     };

     template<>
     class ResolveBy<std::exception_ptr> {
     public:
          template<typename X>
          static void resolve(const Future<X> &fut, const std::exception_ptr &ptr) {
               fut.reject(ptr);
          }
     };

}

template<typename T>
void Future<T>::resolve(T &&val) const {
     if (!resolved()) {
          state->value = val;
          flushCallbacks(nullptr);
     }
}

template<typename T>
template<typename X>
void Future<T>::resolve(X &&val) const {
     _details::ResolveBy<X>::resolve(*this, std::forward<X>(val));
}

template<typename T>
void Future<T>::reject(const std::exception_ptr &exp) const {
     if (!resolved()) {
          state->exception = exp;
          flushCallbacks(nullptr);
     }
}


template<typename T>
template<typename Fn>
inline void Future<T>::addCallback(Fn &fn) const {
     class CB: public Callback {
     public:
          Fn fn;
          CB(Fn &&fn):fn(std::move(fn)) {}
          virtual void call(const Future &fut) noexcept {
               fn(fut);
          }
     };

     if (state->resolved.load()) {
          fn(FutureResolved<T>(*this,true));
     }
     CB *p = new CB(std::move(fn));
     Callback *nx = state->callbacks.load();
     do {
          p->next = nx;
     } while (!state->callbacks.compare_exchange_strong(nx, p));
     if (state->resolved.load()) {
          flushCallbacks(p);
     }
}

template<typename T>
inline Future<T>::Future(T &&value):state (new State) {
     resolve(std::move(value));
}

template<typename T>
inline void Future<T>::flushCallbacks(const Callback *cb) const {
     state->resolved.store(true);
     _details::FutureSemaphore *s = state->semaphore.load();
     if (s != nullptr) {
          s->cond.notify_all();
     }
     Callback *z = nullptr;
     state->callbacks.exchange(z);
     while (z) {
          Callback *p = z;
          z = z->next;
          p->call(FutureResolved<T>(*this,p == cb));
          delete p;
     }

}

template<typename RetVal>
struct FutureReturnT { using T = Future<std::remove_reference_t<RetVal> >;};
template<typename RetVal>
struct FutureReturnT<Future<RetVal> > { using T = Future<RetVal>;};
template<>
struct FutureReturnT<void> { using T = void;};

template<typename RetVal> using FutureReturn = typename FutureReturnT<RetVal>::T;




template<typename T>
template<typename Fn, typename ProcessFn>
inline typename _details::FutureCBBuilder<T>::RetVal _details::FutureCBBuilder<T>::build(Fn &&fn, ProcessFn &&pfn) {
     Future<T> r;
     pfn([r, fn = std::move(fn)](const auto &val){
          try {
               r.resolve(fn(val));
          } catch (...) {
               r.reject(std::current_exception());
          }
     });
     return r;
}

template<typename T>
template<typename Fn, typename ProcessFn>
inline typename _details::FutureCBBuilder<Future<T> >::RetVal _details::FutureCBBuilder<Future<T> >::build(Fn&& fn, ProcessFn&& pfn) {
     Future<T> r;
     pfn([r, fn = std::move(fn)](const auto &val){
          try {
               Future<T>  fut = fn(val);
               fut >> [r](const auto &f2) {
                    try {
                         r.resolve(f2.get());
                    } catch (...) {
                         r.reject(std::current_exception());
                    }
               };
          } catch (...) {
               r.reject(std::current_exception());
          }
     });
     return r;
}

template<typename Fn, typename ProcessFn>
inline typename _details::FutureCBBuilder<void>::RetVal _details::FutureCBBuilder<void>::build(Fn&& fn, ProcessFn&& pfn) {
     pfn(std::forward<Fn>(fn));
}



template<typename T>
inline Future<T>::State::~State() {
     auto p = callbacks.exchange(nullptr);
     while (p) {
          auto z = p;
          p = p->next;
          delete z;
     }
     _details::FutureSemaphore *x = semaphore.load();
     if (x != nullptr) {
          delete x;
     }
}

template<typename T>
const T &Future<T>::get() const {
     while (!resolved()) {
          wait();
     }
     if (state->exception != nullptr) {
          std::rethrow_exception(state->exception);
          throw;
     }
     return *state->value;
}

template<typename T>
inline typename Future<T>::Semaphore* Future<T>::installSemaphore() const {
     Semaphore *x = state->semaphore.load();
     if (x == nullptr) {
          Semaphore *z = new Semaphore;
          if (state->semaphore.compare_exchange_strong(x, z)) {
               x = z;
          } else {
               delete z;
          }
     } else {
     }
     return x;
}


template<typename T>
template<typename A, typename B>
inline bool Future<T>::wait_for(const std::chrono::duration<A, B> &period) const {
     bool res;
     if (!resolved()) {
          Semaphore *sem = installSemaphore();
          std::unique_lock _(sem->mx);
          res = sem->cond.wait_for(_, period, [&]{return resolved();});
     } else {
          res = true;
     }
     return res;
}

template<typename T>
template<typename A, typename B>
inline bool Future<T>::wait_until(const std::chrono::time_point<A, B> &t) const {
     bool res;
     if (!resolved()) {
          Semaphore *sem = installSemaphore();
          std::unique_lock _(sem->mx);
          res = sem->cond.wait_until(_, t, [&]{return resolved();});
     } else {
          res = true;
     }
     return res;
}

template<typename T>
void Future<T>::wait() const {
     if (!resolved()) {
          Semaphore *sem = installSemaphore();
          std::unique_lock _(sem->mx);
          sem->cond.wait(_, [&]{return resolved();});
     }
}

template<typename T>
void Future<T>::uninstallSemaphore() const {
     _details::FutureSemaphore *x = state->semaphore.exchange(nullptr);
     if (x != nullptr) {
          delete x;
     }
}

template<typename T>
bool Future<T>::defined() const {
     return state != nullptr;
}

template<typename T>
inline FutureResolved<T>::FutureResolved(const Future<T> &f, bool nested):Future<T>(f),nested(nested)

{
}



}


#endif /* SRC_SHARED_FUTURE_H_ */
