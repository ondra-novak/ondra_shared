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
	auto operator>>(Fn &&fn) -> typename _details::FutureCBBuilder<decltype(fn(std::declval<Future>()))>::RetValue {
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
	void resolve(const T &val);
	///Resolves the future with value
	/** @note function is not MT safe
	 *
	 * @param val value to set
	 */
	void resolve(T &&val);
	///Rejects the future with an exception
	void reject(const std::exception_ptr &exp);
	///Waits for resolution
	void wait() const;
	///Returns true, when future is already resolved
	bool resolved() const;

	class Callback {
	public:
		virtual void call(const Future &fut) noexcept = 0;
		virtual ~Callback() {};
		Callback *next = nullptr;
	};



protected:

	class State: public RefCntObj {
	public:
		std::optional<T> value;
		std::exception_ptr exception;
		std::atomic<Callback *> callbacks;
		~State();
	};

	static constexpr Callback * resolved_ptr = reinterpret_cast<Callback *>(std::intptr_t(-1));

	using PState = ondra_shared::RefCntPtr<State>;

	PState state;
	template<typename Fn>
	void addCallback(Fn &fn);
	void flushCallbacks();
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
bool Future<T>::resolved() const {
	return state->value.has_value() || state->exception != nullptr;
}

template<typename T>
void Future<T>::resolve(const T &val) {
	if (!resolved()) {
		state->value = val;
		flushCallbacks();
	}
}

template<typename T>
void Future<T>::resolve(T &&val) {
	if (!resolved()) {
		state->value.emplace(std::move(val));
		flushCallbacks();
	}
}

template<typename T>
void Future<T>::reject(const std::exception_ptr &exp) {
	if (!resolved()) {
		state->exception = exp;
		flushCallbacks();
	}
}


template<typename T>
template<typename Fn>
inline void Future<T>::addCallback(Fn &fn) {
	class CB: public Callback {
	public:
		Fn fn;
		CB(Fn &&fn):fn(std::move(fn)) {}
		virtual void call(const Future &fut) noexcept {
			fn(fut);
		}
	};
	Callback *nx = resolved;
	if (state->callbacks.compare_exchange_string(resolved, nullptr)) {
		fn(*this);
		flushCallbacks();
	} else {
		CB *p = new CB(std::move(fn));
		do {
			if (nx == resolved_ptr) {
				p->next = nullptr;
				if (state->callbacks.compare_exchange_strong(nx, p)) {
					flushCallbacks();
					return;
				}
			}
			p->next = nx;
		} while (!state->callbacks.compare_exchange_weak(nx, p));
	}
}

template<typename T>
inline Future<T>::Future(T &&value):state (new State) {
	resolve(std::move(value));
}

template<typename T>
inline void Future<T>::flushCallbacks() {
	Callback *z = nullptr;
	do {
		state->callbacks.exchange(z);
		while (z && z != resolved_ptr) {
			Callback *p = z;
			z = z->next;
			p->call(*this);
			delete p;
		}
	} while (!state->callbacks.compare_exchange_weak(z, resolved_ptr));
}


#endif /* SRC_SHARED_FUTURE_H_ */


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
	while (p && p != resolved_ptr) {
		auto z = p;
		p = p->next;
		delete z;
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
	return state->value.get();
}


}

