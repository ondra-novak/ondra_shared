/*
 * future.h
 *
 *  Created on: 28. 11. 2017
 *      Author: ondra
 */

#ifndef _ONDRA_SHARED_FUTURE_H_2390874612087
#define _ONDRA_SHARED_FUTURE_H_2390874612087
#include <exception>


namespace ondra_shared {


namespace _details {

template<typename T, typename Fn> class FutureChaining;


}



///Different implementation of the future variable
/** difference from std::future - there is no promise object. The future
 * is single object which can either contain or not yet contain value.
 *
 * Reading value when there is no value yet causes blocking of the current thread.
 * You can also install a watch handler which is called when the value is set.
 */
template<typename T>
class Future {
public:

	typedef std::function<void(const Future<T> &)> WatchHandler;

	typedef T ValType;



	///Future can be initialized only as empty
	Future();
	///Copying is enabled however it only copies a state of the value
	/** The construction is equivalent of
	 * @code
	 * Future<...> src(...);
	 * Future<...> trg;
	 * src.tryGet([&](v){trg=v;});
	 * @endcode
	 *
	 * @param other
	 */
	Future(const Future &other);
	///Initializes with value
	/** Equivalent to
	 * Future<...> s;
	 * s=other;
	 * @endcode
	 *
	 * @param other
	 */
	Future(const T &other);
	Future(T &&other);


	~Future();

	Future &operator=(const Future &other);
	Future &operator=(T &&other) noexcept;
	Future &operator=(const T &other);
	Future &operator=(const std::exception_ptr &exception);

	///returns true, if future has value
	/** @retval true future has value
	 *  @retval false future has not value yet
	 *
	 */
	bool hasValue() const;
	///Retrieves value. Blocks thread if there is no value yet
	/**
	 * @return
	 */
	const T &get() const;

	///Waits for value
	void wait() const;

	///waits for value for specified duration
	template<typename Duration> bool wait_for(const Duration &dur);
	///waits for the value until specified time
	template<typename TimePoint> bool wait_until(const TimePoint &dur);

	///Install watch function
	/**
	 * The function is called when the value is set.
	 * The function is called in context of the thread which is setting the value.
	 * The function is not called when the value already has the value. See return value
	 *
	 * @param fn function with simple prototype void(const T &value)
	 * @retval true installed
	 * @retval false not installed, the future is already resolved (has value)
	 */
	bool watch(const WatchHandler &handler);
	///Tries to get value
	/**
	 * @retval nullptr The future has no value yet
	 * @retval pointer The pointer to value
	 */
	const T *tryGet() const;


	template<typename Fn>
	typename _details::FutureChaining<T, Fn>::RetVal operator>>(Fn &&fn) {
		return _details::FutureChaining<T, Fn>::RetVal::chain(this, std::forward<Fn>(fn));
	}


	std::exception_ptr getException() const {
		Lock _(lock);
		return this->exceptPtr;
	}


	///Future resolved when all futures in the range are resolved
	/**
	 * Function iterates through the object and uses operator >> on everyone. Once
	 * the all futures are resolved, the current future is resolved with argument beg
	 *
	 * @param beg begin of range
	 * @param end end of range
	 *
	 * it is possible to wait on futures of many types. It needs apropriate iterator
	 */
	void all(T beg, const T &end);

	///Future resolved when any futures from the range is resolved
	/**
	 * Function iterates through the object and uses operator >> on everyone. Once
	 * the any futures is resolved, the current future is resolved with the iterator
	 * of that future
	 *
	 * @param beg begin of range
	 * @param end end of range
	 *
	 * it is possible to wait on futures of many types. It needs apropriate iterator
	 */
	void any(T beg, const T &end);

protected:
	typedef std::vector<WatchHandler> Handlers;
	typedef std::unique_lock<std::recursive_mutex> Lock;
	///Initialized to nullptr, but once has value, the pointer is set
	const T *valuePtr = nullptr;
	///Initialized to nullptr, but if exception is stored, the pointer is set
	std::exception_ptr exceptPtr;
	///List of watch handlers;
	Handlers handlers;
	///lock protect internals
	std::recursive_mutex lock;
	///condition variable
	std::condition_variable cond;
	///storage for value;
	unsigned char storage[sizeof(T)];


	bool hasValueLk() const;

	void notifyLk();



};


class FutureDestroyed: public std::exception {
public:
	virtual const char *what() const throw() override {return "Future destroyed";}
};

template<typename T>
class SharedFuture: public std::shared_ptr<Future<T> > {
public:
	typedef std::shared_ptr<Future<T> > Super;
	typedef typename Future<T>::ValType ValType;

	using std::shared_ptr<Future<T> >::shared_ptr;

	SharedFuture &operator=(const SharedFuture &other) {
		Super::operator=(other);
		return *this;
	}
	SharedFuture &operator=(const Future<T> &other) {
		Super::operator=(other);
		return *this;
	}
	SharedFuture &operator=(T &&other) {*this->get() = other;return *this;}
	SharedFuture &operator=(const T &&other) {*this->get() = other;return *this;}
	SharedFuture &operator=(const std::exception_ptr &exception) {*this->get() = exception;return *this;}

	///returns true, if future has value
	/** @retval true future has value
	 *  @retval false future has not value yet
	 *
	 */
	bool hasValue() const {return this->get()->hasValue();}
	///Retrieves value. Blocks thread if there is no value yet
	/**
	 * @return
	 */
	const T &get() const {return this->get()->get();}

	///Waits for value
	void wait() const  {return this->get()->wait();}

	///waits for value for specified duration
	template<typename Duration> bool wait_for(const Duration &dur)  {return this->get()->wait_for(dur);}
	///waits for the value until specified time
	template<typename TimePoint> bool wait_until(const TimePoint &tp) {return this->get()->wait_until(tp);}

	///Install watch function
	/**
	 * The function is called when the value is set.
	 * The function is called in context of the thread which is setting the value.
	 * The function is not called when the value already has the value. See return value
	 *
	 * @param fn function with simple prototype void(const T &value)
	 * @retval true installed
	 * @retval false not installed, the future is already resolved (has value)
	 */
	bool watch(const typename Future<T>::WatchHandler &handler) {return this->get()->watch(handler);}
	///Tries to get value
	/**
	 * @retval nullptr The future has no value yet
	 * @retval pointer The pointer to value
	 */
	const T *tryGet() const  {return this->get()->tryGet();}


	///Allows to chaining futures
	/** Using operator >> is similar to the function watch(). You can install watch handler
	 * which is called when the value is set. There is only one significant change. The
	 * chained handler is called always whereas the watch handler only if it is
	 * installed before the future resolution. The watch handler is always called in context
	 * of thread which resolved the promise (or it is never called), the chained handler
	 * can be called in context of current thread - because it is imposible to install
	 * handler on already resolved future, in this case, the handler is called directly instead.
	 *
	 * So if you need to care about context of calling, don't use the chained handlers. If you
	 * don't care about context, you can chain the handler.
	 *
	 * Other difference from the watch is that the handler can return a value. In case
	 * that value is returned, it can be obtained as SharedFuture. The instance
	 * of SharedFuture also supports chaining, so you can chain additional handler and so on
	 *
	 *
	 * @param fn Function called when the future is resolved. The function must accept
	 * one argument... the future itself. The function can return value. If it does
	 * return, it can be obtained as SharedFuture. If it returns SharedFuture, returned
	 * value is SharedFuture which is resolved when the returned SharedFuture is resolved
	 *
	 * @return function returns void when the handler has no result, Otherwise
	 * it returns SharedFuture which is resolved when the result is ready
	 */
	template<typename Fn>
	typename _details::FutureChaining<T, Fn>::RetVal operator>>(const Fn &fn) {
		return _details::FutureChaining<T, Fn>::RetVal::chain(this->get(), fn);
	}
};




template<typename T>
inline Future<T>::Future() {}

template<typename T>
inline Future<T>::Future(const Future& other) {
	const T *val = other.tryGet();
	if (val) this->operator =(*val);
}

template<typename T>
inline Future<T>::Future(const T& other) {
	this->operator =(other);
}

template<typename T>
inline Future<T>::Future(T&& other) {
	this->operator =(std::move(other));
}

template<typename T>
inline Future<T>::~Future() {
	//destroying future - it is not easy as it looks
	//first we need to get lock
	Lock _(lock);
	//in case that future is already resolved
	if (hasValueLk()) {
		//if there is a value, destroy it
		if (valuePtr) valuePtr->~T();
		//destroy exception ptr (under lock)
		exceptPtr = nullptr;
		//finished
	} else {
		//in case that future is not resolved we need to send exception to all listeners
		this->exceptPtr = std::make_exception_ptr((FutureDestroyed()));
		//notify all listeners
		notifyLk();
		//unlock now, so any threads waiting for the results can continue
		_.unlock();
		//notify again (last call)
		cond.notify_all();
		//lock the mutex to ensure that nobody accessing the future
		_.lock();
		//continue in destruction
	}
}

template<typename T>
inline Future<T>& Future<T>::operator =(const Future& other) {
	const T *val = other.tryGet();
	if (val) this->operator =(*val);
}

template<typename T>
inline Future<T>& Future<T>::operator =(T&& other) noexcept {
	Lock _(lock);
	if (hasValueLk()) return *this;
	valuePtr = new((void *)storage) T(std::move(other));
	notifyLk();
	return *this;
}

template<typename T>
inline Future<T>& Future<T>::operator =(const T& other) {
	Lock _(lock);
	if (hasValueLk()) return *this;
	valuePtr = new((void *)storage) T(other);
	for (auto &&x:handlers) {
		x();
	}
	Handlers h;
	std::swap(h, handlers);
	return *this;
}

template<typename T>
inline Future<T>& Future<T>::operator =(const std::exception_ptr& exception) {
	Lock _(lock);
	if (hasValueLk()) return *this;
	exceptPtr = exception;
	for (auto &&x:handlers) {
		x();
	}
	Handlers h;
	std::swap(h, handlers);
	return *this;
}

template<typename T>
inline bool Future<T>::hasValue() const {
	Lock _(lock);
	return hasValueLk();
}

template<typename T>
inline const T& Future<T>::get() const {
	Lock _(lock);
	cond.wait(_,[&]{return hasValueLk();});
	if (exceptPtr != nullptr) std::rethrow_exception(exceptPtr);
	return *valuePtr;
}

template<typename T>
inline void Future<T>::wait() const {
	Lock _(lock);
	cond.wait(_,[&]{return hasValueLk();});
}

template<typename T>
template<typename Duration>
inline bool Future<T>::wait_for(const Duration& dur) {
	Lock _(lock);
	cond.wait_for(_,dur,[&]{return hasValueLk();});
}

template<typename T>
template<typename TimePoint>
inline bool Future<T>::wait_until(const TimePoint& tp) {
	Lock _(lock);
	cond.wait_until(_,tp,[&]{return hasValueLk();});
}

template<typename T>
inline bool Future<T>::watch(const WatchHandler& handler) {
	Lock _(lock);
	if (hasValueLk()) return false;
	handlers.push_back(handler);
	return true;
}

template<typename T>
inline const T* Future<T>::tryGet() const {
	if (!hasValue()) return false;
	if (exceptPtr != nullptr) std::rethrow_exception(exceptPtr);
	return valuePtr;
}

template<typename T>
inline bool Future<T>::hasValueLk() const {
	return valuePtr != nullptr || exceptPtr != nullptr;
}

template<typename T>
inline void Future<T>::all(T beg, const T& end) {

	class SharedState {
		Future<T> *me;
		T beg;
		std::atomic<unsigned int> count;
		SharedState(Future<T> *me,const T &beg )
			:me(me),beg(beg),count(0) {}
	};

	std::shared_ptr<SharedState> shrd = new SharedState(this,beg);
	++shrd->count;
	while (beg != end) {
		++shrd->count;
		(*beg) >> [shrd] {
			if (--shrd->count == 0) (*shrd->me) = shrd->beg;
		};
		++beg;

	}
	if (--shrd->count == 0)
		(*shrd->me) = shrd->beg;
}

template<typename T>
inline void Future<T>::any(T beg, const T& end) {

	while (beg != end) {
		(*beg) >> [this,beg] {
			(*this) = beg;
		};
		++beg;
	}
}

template<typename T>
inline void Future<T>::notifyLk() {
	for (auto &&x:handlers) {
		x(*this);
	}
	Handlers h;
	std::swap(h, handlers);
	cond.notify_all();

}

namespace _details {

template<typename T>
class FutureChaining<T,void()> {
public:
	typedef void RetVal;

	template<typename Fn>
	void chain(Future<T> *future, Fn fn ) {

		auto fcall = [fn] (const Future<T> &) {fn();};
		if (!future->watch(fcall)) fn();
	}
};


///installs watch handler
/**
 * This class is used when future >> void(const Future<T> &) is used
 *
 * It simply installs the handler, however if called on resolved future, the handler
 * is called immediatelly in the context of current thread
 */
template<typename T>
class FutureChaining<T,void(const Future<T> &)> {
public:
	typedef void RetVal;

	template<typename Fn>
	void chain(Future<T> *future, Fn fn ) {

		if (!future->watch(fn)) fn(*future);
	}
};

///installs watch handler and allows chaining
/**
 * This class is used when future >> Ret(const Future<T> &) is used
 *
 * It installs handler and returns SharedFuture which is resolved by the result
 * of the handler. If installed on already resolved future, the handler is called
 * immediatelly
 *
 *
 */
template<typename T, typename Ret>
class FutureChaining<T,Ret(const Future<T> &)> {
public:
	typedef SharedFuture<Ret> RetVal;

	template<typename Fn>
	RetVal chain(Future<T> *future, Fn fn ) {
		SharedFuture<Ret> ret(new Future<Ret>);
		auto fcall = [ret,fn] (const Future<T> &f) {
			try {
				*ret = fn(f);
			} catch (...) {
				*ret = std::current_exception();
			}
		};
		if (!future->watch(fcall)) fcall(*future);
		return ret;

	}
};


///installs watch handler and allows chaining
/**
 * This class is used when future >> SharedFuture<Ret>(const Future<T> &) is used
 *
 * It installs handler and returns SharedFuture which is resolved by the result
 * of the handler. Because handler returns SharedFuture<Ret>, the future is not
 * immediatelly resolved, it resolves once the returning future is also resolved
 *
 * This allows to return future as result of chain
 *
 */

template<typename T, typename Ret>
class FutureChaining<T,SharedFuture<Ret>(const Future<T> &)> {
public:
	typedef SharedFuture<Ret> RetVal;

	template<typename Fn>
	RetVal chain(Future<T> *future, Fn fn ) {
		SharedFuture<Ret> ret(new Future<Ret>);

		auto fcall = [ret,fn](const Future<T> &f) {
			try {
				fn(f) >> [ret](Future<Ret> &res) {*ret = res;};
			} catch (...) {
				*ret = std::current_exception();
			}
		};
		if (!future->watch(fcall)) fcall(*future);
		return ret;

	}
};


template<typename T, typename Ret>
class FutureChaining<T,Future<Ret> > {
public:
	typedef Future<Ret> &RetVal;

	template<typename Fn>
	RetVal chain(Future<T> *future, Future<Ret> &t ) {
		(*future) >> [t](const Future<T> &f) {
			std::exception_ptr p = f.getException();
			if (p != nullptr) t=p;
			else t=f.get();
		};
		return t;
	}
};


template<typename T, typename Ret>
class FutureChaining<T,SharedFuture<Ret> > {
public:
	typedef Future<Ret> &RetVal;

	template<typename Fn>
	RetVal chain(Future<T> *future, const SharedFuture<Ret> &t ) {
		SharedFuture<Ret> ret = t;
		(*future) >> [ret](const Future<T> &f) {
			std::exception_ptr p = f.getException();
			if (p != nullptr) ret=p;
			else ret=f.get();
		};
		return ret;
	}
};


}




}



#endif /* _ONDRA_SHARED_FUTURE_H_2390874612087 */
