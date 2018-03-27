#ifndef _ONDRA_SHARED_FUTURE_H_239087461782087
#define _ONDRA_SHARED_FUTURE_H_239087461782087

#include <condition_variable>
#include <mutex>

namespace ondra_shared {


template<typename FT> class FutureExceptionalState;
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


	template<typename Fn> class CallFnSetValue;

	///Constructs uninitialized future variable
	FutureValue() {}
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
	FutureValue(const FutureValue &value) {
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

	///Blocks execution until the future is resolved or timeout period ellapses which comes first
	/**
	 * @param dur duration
	 * @retval true resolved
	 * @retval false timeout
	 */
	template<typename Dur> bool wait_for(Dur &&dur);

	///Blocks execution until the future is resolved or timeout period ellapses which comes first
	/**
	 * @param dur duration
	 * @retval true resolved
	 * @retval false timeout
	 */
	template<typename Time> bool wait_until(Time &&dur) ;

	///Asynchronous waiting
	/**
	 * Schedules function for execution after future is resolved
	 * @param fn function to execute
	 * @retval true the future is already resolved
	 * @retval false the future is nor resolved, the function is scheduled
	 */
	template<typename Fn> bool await(Fn &&fn);

	///Asynchronous waiting
	/**
	 * Schedules function for execution after future is resolved
	 * @param fn function to execute
	 * @param exceptFn function to execute when future is rejected
	 * @retval true the future is already resolved
	 * @retval false the future is nor resolved, the function is scheduled
	 */
	template<typename Fn, typename ExceptFn> bool await(Fn &&fn, ExceptFn &&exceptFn);

	template<typename ExceptFn> bool await_catch(ExceptFn &&exceptFn);

	template<typename Fn> bool then(Fn &&fn);

	template<typename Fn, typename ExceptFn> bool then(Fn &&fn, ExceptFn &&exceptFn);

	template<typename ExceptFn> bool then_catch(ExceptFn &&exceptFn);

	template<typename Fn> CallFnSetValue<Fn> operator <<=(Fn &&fn);

	template<typename Arg>	void operator << (Arg && arg);

	template<typename Fn> void operator >> (Fn && arg);

	FutureExceptionalState<FutureValue<T> > operator !() ;

	bool is_resolved() const;

	bool is_rejected() const;


protected:

	class AbstractWatchHandler;
	typedef AbstractWatchHandler *PWatchHandler;

	class AbstractWatchHandler {
	public:
		virtual void onValue(const T &value) noexcept(true) = 0;
		virtual void onException(const std::exception_ptr &exception) noexcept(true) = 0;
		virtual void release() {delete this;}
		virtual ~AbstractWatchHandler() {}
		PWatchHandler next;
	};


	void addHandler(AbstractWatchHandler *h);
	bool removeHandler(AbstractWatchHandler *h);


	typedef std::unique_lock<std::mutex> Sync;

	PWatchHandler watchChain = nullptr;
	const T *value = nullptr;
	std::exception_ptr exception;
	unsigned char valueBuffer[sizeof(T)];
	std::mutex lock;

	template<typename Fn, typename ValueType>
	void resolve(const Fn &fn, const ValueType &value);

	class WaitHandler;
	template<typename Fn> class WaitHandlerAwait;
	template<typename Fn1, typename Fn2> class WaitHandlerAwaitCatch;
	template<typename Fn> class WaitHandlerCatch;

	template<typename Fn> static auto callCb(Fn &&fn, const T &t) -> decltype(fn(t)) {
		return fn(t);
	}
	template<typename Fn> static auto callCb(Fn &&fn, const T &t) -> decltype(fn()) {
		return fn();
	}
	template<typename Fn> static auto callCbExcept(Fn &&fn, const std::exception_ptr &t) -> decltype(fn(t)) {
		return fn(t);
	}


};
template<typename T> class Future;


template<typename T, typename X>
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

	template<typename Fn> using RetFnThen = typename FutureFromType<T, decltype(std::declval<Fn>()(std::declval<T>()))>::type;
	template<typename Fn> using RetFnCatch = typename FutureFromType<T, decltype(std::declval<Fn>()(std::declval<std::exception_ptr>()))>::type;
	template<typename Fn> using RetFnThenVoid = typename FutureFromType<T, decltype(std::declval<Fn>()())>::type;


	///Create and initialize future variable (the shared state)
	Future();
	///Create uninitialized future variable. It cannot be used until someone assign it a value
	Future(std::nullptr_t) {}
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
	///Wait for specified time
	template<typename Dur> bool wait_for(Dur &&dur);
	///Wait until specified time
	template<typename Time> bool wait_until(Time &&dur);
	///Asynchronous waiting
	/**
	 * Performs waiting asynchronously. Suply the code which is executed once the value is set.
	 *
	 * @param fn function to execite
	 * @retval true future is already resolved, so code will not be executed
	 * @retval false future is not resolved, the code will be executed
	 */
	template<typename Fn> bool await(const Fn &fn);
	///Asynchronous waiting
	/**
	 * Performs waiting asynchronously. Suply the code which is executed once the value is set.
	 *
	 * @param fn function to execute
	 * @param exceptFn function to execute when the value is rejected
	 * @retval true future is already resolved, so code will not be executed
	 * @retval false future is not resolved, the code will be executed
	 */
	template<typename Fn, typename ExceptFn> bool await(const Fn &fn, const ExceptFn &exceptFn);
	///Asynchronous waiting
	/**
	 * Performs waiting asynchronously. Suply the code which is executed once the value is set.
	 *
	 * @param exceptFn function to execute when the value is rejected
	 * @retval true future is already resolved, so code will not be executed
	 * @retval false future is not resolved, the code will be executed
	 *
	 */
	template<typename ExceptFn> bool await_catch(const ExceptFn &exceptFn);
	///Specifies function, which is called upon future resolution.
	/**
	 *
	 * @param fn function to call when future's value is set. The function must accept the future's value as argument.
	 *  The function can return a value of another type or a Future value.
	 * @return Future of return value. If the function throws exception, the returned future becomes rejected

	 * @note rejected state of the source future is directly transfered to returning future

	 */
	template<typename Fn> RetFnThen<Fn> then(const Fn &fn);
	///Specifies function, which is called upon future resolution.
	/**
	 *
	 * @param fn function to call when future's value is set. The function is called without arguments
	 *  The function can return a value of another type or a Future value.
	 * @return Future of return value. If the function throws exception, the returned future becomes rejected

	 * @note rejected state of the source future is directly transfered to returning future

	 */
	template<typename Fn> RetFnThenVoid<Fn> then(const Fn &fn);
	///Specifies function, which is called upon future resolution.
	/**
	 *
	 * @param fn function to call when future's value is set. The function must accept the future's value as argument.
	 *  The function can return a value of another type or a Future value.
	 * @param excepttFn function to call when future is in rejected state. The function must accept exception_ptr.
	 *  The function must return a value of the same type as the fn().
	 * @return Future of return value. If the function throws exception, the returned future becomes rejected


	 */
	template<typename Fn, typename ExceptFn> RetFnThen<Fn> then(const Fn &fn, const ExceptFn &exceptFn);
	///Specifies function, which is called upon future resolution.
	/**
	 *
	 * @param fn function to call when future's value is set. The function is called without arguments
	 *  The function can return a value of another type or a Future value.
	 * @param excepttFn function to call when future is in rejected state. The function must accept exception_ptr.
	 *  The function must return a value of the same type as the fn().
	 * @return Future of return value. If the function throws exception, the returned future becomes rejected


	 */
	template<typename Fn, typename ExceptFn> RetFnThenVoid<Fn> then(const Fn &fn, const ExceptFn &exceptFn);
	///Specifies function, which is called upon the future rejection
	/**
	 * @param excepttFn function to call when future is in rejected state. The function must accept exception_ptr.
	 *  The function must return a value of the same type as the type of the future
	 * @return Future of return value. If the function throws exception, the returned future becomes rejected
	 *
	 * @note if the future is not in rejected state, the value is transfered to the returning future
	 */

	template<typename ExceptFn> Future then_catch(const ExceptFn &exceptFn);

	///Creates a function which - when is called - uses return value of the specifed function to set the future value
	/**
	 *
	 * @param fn function to bind with the future
	 * @return function bound with the future
	 */
	template<typename Fn> typename FutureValue<T>::template CallFnSetValue<Fn> operator <<=(Fn &&fn);

	///Alternative way to set value
	template<typename Arg>	void operator << (Arg && arg);

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


protected:

	PFutureValue v;
};

template<typename T, typename R> class FutureFromType {
public: typedef Future<typename std::remove_reference<R>::type> type;
};
template<typename T, typename R> class FutureFromType<T, Future<R> > {
public: typedef Future<R> type;
};
template<typename T> class FutureFromType<T, std::exception_ptr> {
public: typedef Future<T> type;
};

struct FutureEmptyValue{};

template<>
class Future<void>: public Future<FutureEmptyValue> {

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
void FutureValue<T>::resolve(const Fn &fn, const ValueType &value) {
	while (watchChain != nullptr) {
		PWatchHandler z = watchChain;
		watchChain = z->next;
		lock.unlock();
		(z->*fn)(value);
		z->release();
		lock.lock();
	}
}

template<typename T>
void FutureValue<T>::set(const T &v) {
	Sync _(lock);
	if (is_resolved()) return;

	value = new (valueBuffer) T(v);
	resolve(&AbstractWatchHandler::onValue, *value);
}
template<typename T>
void FutureValue<T>::set(T &&v) {
	Sync _(lock);
	if (is_resolved()) return;

	value = new (valueBuffer) T(std::move(v));
	resolve(&AbstractWatchHandler::onValue, *value);

}

template<typename T>
inline FutureValue<T>::~FutureValue() {
	Sync _(lock);
	if (watchChain != nullptr) {
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
inline bool FutureValue<T>::removeHandler(AbstractWatchHandler *handler) {
	Sync _(lock);
	if (handler == watchChain) {
		PWatchHandler x = watchChain;
		watchChain=watchChain->next;
		x->release();
		return true;
	} else {
		PWatchHandler x = watchChain;
		while (x->next != nullptr && x->next != handler) {
			x= x->next;
		}
		if (x->next != nullptr) {
			PWatchHandler y = x->next;
			x->next = x->next->next;
			y->release();
			return true;
		} else {
			return false;
		}
	}
}

template<typename T>
inline bool FutureValue<T>::is_resolved() const {
	return value != nullptr  || exception != nullptr;
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
	return exception != nullptr;
}


template<typename T>
void FutureValue<T>::reject(const std::exception_ptr &exception) {
	Sync _(lock);
	if (is_resolved()) return;

	this->exception =exception;
	resolve(&AbstractWatchHandler::onException, exception);
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
	virtual void onValue(const T &value) noexcept(true) {
		fired = true;
		condVar.notify_all();
	}
	virtual void onException(const std::exception_ptr &exception) noexcept(true) {
		fired = true;
		condVar.notify_all();
	}
	virtual void release() {}

	std::condition_variable condVar;
	bool fired = false;
};

template<typename T>
inline void FutureValue<T>::wait() const {
	Sync _(lock);
	if (is_resolved()) return;
	WaitHandler wh;
	addHandler(&wh);
	wh.condVar.wait(_,[=]{return wh.fired;});
	//this required because WaitHandler can be removed incidentaly sooner
	//but during resolve phase, when the future is already resolved, but handler
	//has not been notified yet. We need remove handler before the function exits
	removeHandler(&wh);
}



template<typename T>
inline void FutureValue<T>::addHandler(AbstractWatchHandler *h) {

	h->next = watchChain;
	watchChain = h;

}

template<typename T>
template<typename Dur>
inline bool FutureValue<T>::wait_for(Dur&& dur)  {
	Sync _(lock);
	if (is_resolved()) return true;
	WaitHandler wh;
	addHandler(&wh);
	bool res = wh.condVar.wait_for(_,std::forward<Dur>(dur),[=]{return wh.fired();});
	removeHandler(&wh);
	return res;
}

template<typename T>
template<typename Time>
inline bool FutureValue<T>::wait_until(Time&& tm)  {
	Sync _(lock);
	if (is_resolved()) return true;
	WaitHandler wh;
	addHandler(&wh);
	bool res = wh.condVar.wait_until(_,std::forward<Time>(tm),[=]{return wh.fired();});
	removeHandler(&wh);
	return res;
}

template<typename T>
template<typename Fn>
class FutureValue<T>::WaitHandlerAwait: public FutureValue<T>::AbstractWatchHandler {
public:
	WaitHandlerAwait(const Fn &fn):fn(fn) {}
	virtual void onValue(const T &value) noexcept(true) {callCb(fn,value);}
	virtual void onException(const std::exception_ptr &) noexcept(true) {}
private:
	Fn fn;
};

template<typename T>
template<typename Fn>
class FutureValue<T>::WaitHandlerCatch: public FutureValue<T>::AbstractWatchHandler {
public:
	WaitHandlerCatch(const Fn &fn):fn(fn) {}
	virtual void onValue(const T &) noexcept(true) {}
	virtual void onException(const std::exception_ptr &exception) noexcept(true) {callCbExcept(fn,exception);}
private:
	Fn fn;
};

template<typename T>
template<typename Fn1,typename Fn2>
class FutureValue<T>::WaitHandlerAwaitCatch: public FutureValue<T>::AbstractWatchHandler {
public:
	WaitHandlerAwaitCatch(const Fn1 &fn1,const Fn2 &fn2):fn1(fn1),fn2(fn2) {}
	virtual void onValue(const T &value) noexcept(true) {callCb(fn1,value);}
	virtual void onException(const std::exception_ptr &exception) noexcept(true) {callCbExcept(fn2,exception);}
private:
	Fn1 fn1;
	Fn2 fn2;
};


template<typename T>
template<typename Fn>
inline bool FutureValue<T>::await(Fn&& fn) {
	Sync _(lock);
	if (is_resolved()) return true;
	addHandler(new WaitHandlerAwait<Fn>(std::forward<Fn>(fn)));
	return false;
}

template<typename T>
template<typename Fn, typename ExceptFn>
inline bool FutureValue<T>::await( Fn&& fn, ExceptFn&& exceptFn) {
	Sync _(lock);
	if (is_resolved()) return true;
	addHandler(new WaitHandlerAwaitCatch<Fn, ExceptFn>(std::forward<Fn>(fn),std::forward<ExceptFn>(exceptFn)));
	return false;
}

template<typename T>
template<typename ExceptFn>
inline bool FutureValue<T>::await_catch(ExceptFn&& exceptFn) {
	Sync _(lock);
	if (is_resolved()) return true;
	addHandler(new WaitHandlerCatch<ExceptFn>(std::forward<ExceptFn>(exceptFn)));
	return false;
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

template<typename T> template<typename Fn>
class FutureValue<T>::CallFnSetValue {

	template<typename ... Args>
	static void test_fn_void(std::true_type);

public:
	CallFnSetValue(FutureValue<T> &owner, Fn &&fn):owner(owner), fn(std::forward<Fn>(fn)) {}

	template<typename... Args>
	auto operator()(Args &&... args) -> decltype(test_fn_void(std::declval<typename std::is_void<decltype(std::declval<Fn>()(std::forward<Args>(args)...))>::type>())){
		try {
			fn(std::forward<Args>(args)...);owner.set(T());
		} catch (...) {
			owner.reject();
		}
	}
	template<typename... Args>
	auto operator()(Args &&... args) -> decltype(std::declval<FutureValue<T> >().set(std::declval<Fn>()(std::forward<Args>(args)...))) {
		try {
			owner.set(fn(std::forward<Args>(args)...));
		} catch (...) {
			owner.reject();
		}
	}
protected:
	FutureValue<T> &owner;
	Fn fn;
};


template<typename T> template<typename Fn>
typename FutureValue<T>::template CallFnSetValue<Fn> FutureValue<T>::operator <<=(Fn &&fn) {
	return CallFnSetValue<Fn>(*this,std::forward<Fn>(fn));
}

template<typename T>
FutureExceptionalState<FutureValue<T> >  FutureValue<T>::operator !() {
	return FutureExceptionalState<FutureValue<T> >(*this);
}

template<typename FT> class FutureExceptionalState {
public:
	FutureExceptionalState(FT &owner):owner(owner) {}
	template<typename Fn> auto operator >> (Fn &&fn) -> decltype(std::declval<FT>().then_catch(fn)) {
		return owner.then_catch(fn);
	}
	operator bool() const {return !owner.is_resolved();}
	void operator << (const std::exception_ptr &ptr) {
		owner.reject(ptr);
	}
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
template<typename T> Future<T>::Future(nullptr_t) {}
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
	Future<T> me(this);
	value->v->then([=](const T &v){me.set(v);}
	            ,[=](const std::exception_ptr &v) {me.reject(v);});
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
template<typename T> template<typename Dur> bool Future<T>::wait_for(Dur &&dur) {
	return v->wait_for(dur);
}
template<typename T> template<typename Time> bool Future<T>::wait_until(Time &&dur) {
	return v->wait_until(dur);
}
template<typename T> template<typename Fn> bool Future<T>::await(const Fn &fn) {
	return v->await(fn);
}
template<typename T> template<typename Fn, typename ExceptFn> bool Future<T>::await(const Fn &fn, const ExceptFn &exceptFn) {
	return v->await(fn, exceptFn);
}
template<typename T> template<typename ExceptFn> bool Future<T>::await_catch(const ExceptFn &exceptFn) {
	return v->await_catch(exceptFn);
}
template<typename T> template<typename Fn> typename Future<T>::template RetFnThen<Fn> Future<T>::then(const Fn &fn) {
	typename Future<T>::template RetFnThen<Fn> f;
	v->then([f,cfn=Fn(fn)](const T &val) mutable {
		(f<<=cfn)(val);
	},[f](const std::exception_ptr &e) mutable  {
		f.reject(e);
	});
	return f;
}
template<typename T> template<typename Fn, typename ExceptFn> typename Future<T>::template RetFnThen<Fn> Future<T>::then(const Fn &fn, const ExceptFn &exceptFn) {
	typename Future<T>::template RetFnThen<Fn>  f;
	v->then([f,cfn=Fn(fn)](const T &val) mutable {
		(f<<=cfn)(val);
	},[f,efn=ExceptFn(exceptFn)](const std::exception_ptr &e) mutable  {
		(f<<=efn)(e);
	});
	return f;
}
template<typename T> template<typename Fn> typename Future<T>::template RetFnThenVoid<Fn> Future<T>::then(const Fn &fn) {
	typename Future<T>::template RetFnThenVoid<Fn> f;
	v->then([f,cfn=Fn(fn)](const T &) mutable {
		(f<<=cfn)();
	},[f](const std::exception_ptr &e) mutable  {
		f.reject(e);
	});
	return f;
}
template<typename T> template<typename Fn, typename ExceptFn> typename Future<T>::template RetFnThenVoid<Fn> Future<T>::then(const Fn &fn, const ExceptFn &exceptFn) {
	typename Future<T>::template RetFnThenVoid<Fn>  f;
	v->then([f,cfn=Fn(fn)](const T &) mutable {
		(f<<=cfn)();
	},[f,efn=ExceptFn(exceptFn)](const std::exception_ptr &e) mutable {
		(f<<=efn)(e);
	});
	return f;
}
template<typename T> template<typename ExceptFn> Future<T> Future<T>::then_catch(const ExceptFn &exceptFn) {
	return then([=](const T &x) {return x;},exceptFn);
}
template<typename T> template<typename Fn> typename FutureValue<T>::template CallFnSetValue<Fn> Future<T>::operator <<=(Fn &&fn) {
	return typename FutureValue<T>::template CallFnSetValue<Fn>(*v,std::forward<Fn>(fn));
}
template<typename T> template<typename Arg>	void Future<T>::operator << (Arg && arg) {
	set(std::forward<Arg>(arg));
}
template<typename T> template<typename Fn> auto Future<T>::operator >> (Fn && fn) -> decltype(this->then(fn)) {
	return then(fn);
}


template<typename T> bool Future<T>::is_resolved() const {
	return v->is_resolved();
}
template<typename T> bool Future<T>::is_rejected() const {
	return v->is_rejected();
}

}


#endif
