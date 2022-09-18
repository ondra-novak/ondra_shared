/*
 * async_future.h
 *
 *  Created on: 31. 8. 2022
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_ASYNC_FUTURE_H_wdj239edj3u30
#define ONDRA_SHARED_ASYNC_FUTURE_H_wdj239edj3u30

#include <atomic>
#include <exception>
#include <list>
#include <memory>




namespace ondra_shared {

class async_future_common {
protected:

    template<typename Me>
    class AbstractCallback {
    public:
        AbstractCallback *next;
        AbstractCallback(AbstractCallback *next):next(next){};
        AbstractCallback(const AbstractCallback &other) = delete;
        AbstractCallback& operator=(const AbstractCallback &other) = delete;
        virtual void run(const Me &, bool async) noexcept = 0;
        virtual ~AbstractCallback() {
            if (next) delete next;
        }
        void flush_down(const Me &x, bool async) {
            run(x,async);
            if (next) next->flush_down(x,async);
        }
    };

    template<typename Me, typename Fn>
    class Callback: public AbstractCallback<Me> {
    public:
        Callback(Fn &&fn, AbstractCallback<Me> *next)
            :AbstractCallback<Me>(next), fn(std::forward<Fn>(fn)) {}
        virtual void run(const Me &x, bool) noexcept {
            fn(x);
        }
    protected:
        Fn fn;
    };
    template<typename Me, typename Fn>
    class Callback2: public AbstractCallback<Me> {
    public:
        Callback2(Fn &&fn, AbstractCallback<Me> *next)
            :AbstractCallback<Me>(next), fn(std::forward<Fn>(fn)) {}
        virtual void run(const Me &x, bool b) noexcept {
            fn(x,b);
        }
    protected:
        Fn fn;
    };


};

///Asynchronous future - like a future, but waiting is handled through a callback
/**
 * This is very small and simple class implemented such a way in which doesn't enforces
 * any sharing method. It can be allocated statically, or dynamically through the
 * unique or shared ptr. In most complex usage, it is recommended to allocate object
 * using shared_ptr.
 *
 * The main issue is that both sides - producer and consumer must to know the location
 * of the future object. So if there is no such fixed place, you need to allocate
 * the object dynamically
 *
 * However, you can allocate the object statically, attach all necessery consumer callback(s)
 * and then to move the object to the producer. In this case, no shared place is needed
 *
 * @note MT Safety. It is MT safe to read object from many threads as well as install
 * a new callbacks. However it is not MT Safe to produce results from many threads as
 * it is not allowed at all. There must be only one producer. In case that future object
 * is resolved twice, the second attempt generates an exception. However this rule is
 * not always applied especially if the setting value is subject of race condition. So
 * if you need to access from various producers, you need to add extra synchronization.
 * The setting a value is not atomically. Checking ready() status is not enough, this
 * function starts to return true when the final value is settled.
 *
 *
 * @tparam T type of the value. You can use void to get untyped future (which has special
 * features, see futher doc)
 */
template<typename T>
class async_future: public async_future_common {
public:

    ///Construct empty future
    /**This future is not reada yet */
    async_future();
    ///Move the future value
    /**
     * @param other source future to move from
     *
     * @note Moving is not MT Safe. You need to be sure, that nobody can access source
     * instance. The operation moves result (if it is ready), exception state, and/or
     * waiting callbacks.
     */
    async_future(async_future &&other);

    ///Copy the future value
    /**
     * Creates copy of the another future. Note it only copies value and state, not
     * callbacks. If the other future is not ready yet, it constructs not-ready future
     * @param other source future
     */
    async_future(const async_future &other);

    ///Construct future in ready state
    /**
     * This future has already a value
     * @param value new value
     */
    async_future(const T &value);
    ///Construct future in ready state
    /**
     * This future has already a value
     * @param value new value
     */
    async_future(T &&value);
    ///Construct future in ready state with an exception
    /**
     * This future is in exception state
     * @param eptr exception
     */

    async_future(std::exception_ptr eptr);
    
    template<typename Fn, typename = decltype(std::declval<Fn>()(std::declval<const async_future<T> &>()))>
    async_future(Fn &&fn):async_future() {
        this->operator>>(std::forward<Fn>(fn));
    }
    ///Destruction
    /**
     * @note if the future being destroied is not ready yet, all callbacks
     *  all called anyway passing the not-ready future as an argument. The callbacks
     *  must not read value, otherwise an exception is generated which can
     *  cause termination of the program. The callback should always test state of the
     *  future using the function is_ready()
     */
    ~async_future();

    ///Move value of one future to other
    /**
     * @param other other value. If the other future is not ready yet, operator only
     * merges all callbacks. If the other future is ready yet, the value is
     * used to resolve this future.
     * @return this
     * @exception async_future_already_resolved Attempt to assign value to already resolved
     * future.
     */
    async_future &operator=(async_future &&other);
    ///Copy value of one future to other
    /**
     * @param other other value. If the other future is not ready yet, the operator does
     * nothing. If the other future is ready yet, the value is used to resolve this future.
     * @return this
     * @exception async_future_already_resolved Attempt to assign value to already resolved
     * future.
     */
    async_future &operator=(const async_future &other);
    ///Assign a value to the future and mark it as resolved
    /**
     * @param value new value
     * @return this;
     * @exception async_future_already_resolved Attempt to assign value to already resolved
     * future.
     */
    async_future &operator=(const T &value);
    ///Assign a value to the future and mark it as resolved
    /**
     * @param value new value
     * @return this;
     * @exception async_future_already_resolved Attempt to assign value to already resolved
     * future.
     */
    async_future &operator=(T &&value);
    ///Assign an exception state to the future and mark it as resolved
    /**
     * @param eptr an exception ptr
     * @return this;
     * @exception async_future_already_resolved Attempt to assign value to already resolved
     * future.
     */
    async_future &operator=(std::exception_ptr eptr);

    ///Request the future value
    /**
     * @return the future value
     * @exception async_future_not_ready the future is not ready yet
     * @exception any operator also throws stored exception if the future is in
     * exception state.
     *
     */
    operator const T &() const;

    /** @{ */
    ///Attach new callback
    /**
     * @param cb callback function, which accepts the const reference to the future object
     *
     * You can choose two prototypes
     *
     * @code
     * void(const async_future &value);
     * void(const async_future &value, bool async);
     * @endcode
     *
     * The second variant contains bool async, which is set true if the callback
     * is being called asynchronously and set to false when it is called synchronously.
     * The synchronous call means in context of this function, which also means, that
     * callback is called immediately after attach, because the future is already
     * resolved, so it is called in the context of the current thread. This helps you to manage
     * control flow. You probably don't need continue in cycle recursively if you know,
     * that call is synchronous
     *
     * @note if the future is already resolved, the callback function is immediatelly called,
     * otherwise it is registered and can be called when the future is resolved.
     *
     * The callback function receives reference to this future. The callback function should
     * not assume, that future is always ready. If the future is destroyed before
     * it is resolved, the callback function is called with the not ready yet future (meaning
     * that future will be never resolved)
     */
    template<typename Fn>
    auto operator>>(Fn &&cb) -> decltype(std::declval<Fn>()(std::declval<const async_future<T> &>()), std::declval<async_future &&>()) {
    	addfn1(std::forward<Fn>(cb));
    	return std::move(*this);
    }

    template<typename Fn>
    auto operator>>(Fn &&cb) -> decltype(std::declval<Fn>()(std::declval<const async_future<T> &>(), std::declval<bool>()), std::declval<async_future &&>()) {
    	addfn2(std::forward<Fn>(cb));
    	return std::move(*this);
    }

    /** }@ **/

    ///Determines whether future is ready
    /**
     * @retval true future is ready - this is final state (will not change)
     * @retval false future is not ready, however this state can change anytime later. Do
     * not poll the state. Use callback
     */
    bool is_ready() const;

    ///Determines whethe future is not ready yet
    /**
     * @code
     * async_future<int> v...;
     *
     * if (!v) v >> [=](const async_future<int> &w) { ...asynchronous access ...};
     * else { ...synchronous access... }
     *
     * @endcode
     *
     * @return true future is not ready yet
     * @retval false future is ready
     */
    bool operator !() const {return !is_ready();}


protected:

    using AbstractCallback = async_future_common::AbstractCallback<async_future<T> >;


    std::atomic<bool> _resolved;
    std::atomic<AbstractCallback *> _callbacks;

    bool _is_exception;
    union {
        T _value;
        std::exception_ptr _eptr;
    };

    void mark_resolved();
    void flush_cbs(bool async);
    AbstractCallback *add_cb(AbstractCallback *itm, bool async);

    template<typename Fn>
    void addfn1(Fn &&cb);

    template<typename Fn>
    void addfn2(Fn &&cb);

};


///Future is not ready yet - thrown when asking for a value
class async_future_not_ready: public std::exception {
public:
    const char *what() const noexcept {return "async_future is not ready yet";}
};

///Future already has a value - thrown when assignment
class async_future_already_resolved: public std::exception {
public:
    const char *what() const noexcept {return "async_future is already resolved";}
};


template<typename T>
inline async_future<T>::async_future()
:_resolved(false),_callbacks(nullptr),_is_exception(false)
{
}

template<typename T>
inline async_future<T>::async_future(async_future &&other)
:_resolved(other.is_ready())
,_callbacks(other._callbacks.load())
,_is_exception(other._is_exception)
{
    other._callbacks = nullptr;
    if (is_ready()) {
        if (_is_exception) {
            new(&_eptr) std::exception_ptr(std::move(other._eptr));
        } else {
            new(&_value) T(std::move(other._value));
        }
    }
}

template<typename T>
inline async_future<T>::async_future(const async_future &other)
:_resolved(other.is_ready())
,_callbacks(nullptr)
,_is_exception(other._is_exception)
{
    if (is_ready()) {
        if (_is_exception) {
            new(&_eptr) std::exception_ptr(other._eptr);
        } else {
            new(&_value) T(other._value);
        }
    }
}

template<typename T>
inline async_future<T>::async_future(const T &value)
    :_resolved(true)
    ,_callbacks(nullptr)
    ,_is_exception(false)
    ,_value(value) {}

template<typename T>
inline async_future<T>::async_future(T &&value)
    :_resolved(true)
    ,_callbacks(nullptr)
    ,_is_exception(false)
    ,_value(std::move(value)) {}

template<typename T>
inline async_future<T>::async_future(std::exception_ptr eptr)
:_resolved(true)
,_callbacks(nullptr)
,_is_exception(true)
,_eptr(eptr) {}



template<typename T>
inline async_future<T>::~async_future() {
    AbstractCallback *z = _callbacks.exchange(nullptr, std::memory_order_relaxed);
    if (z) {
        z->flush_down(*this, true);
        delete z;
    }

    if (is_ready()) {
        if (_is_exception) {
            _eptr.~exception_ptr();
        } else {
            _value.~T();
        }
    }
}

template<typename T>
inline async_future<T>& async_future<T>::operator =(async_future &&other) {
    if (&other != this) {
        if (is_ready()) throw async_future_already_resolved();
        if (other.is_ready()) {
            _is_exception = other._is_exception;
            if (other._is_exception) {
                new(&_eptr) std::exception_ptr(std::move(other._eptr));
            } else {
                new(&_value) T(std::move(other._value));
            }
            mark_resolved();
        } else {
            AbstractCallback *l = other._callbacks.exchange(nullptr);
            while (l) l = add_cb(l);
        }
    }
    return *this;
}

template<typename T>
inline async_future<T>& async_future<T>::operator =(const async_future &other) {
    if (&other != this) {
        if (is_ready()) throw async_future_already_resolved();
        if (other.is_ready()) {
            _is_exception = other._is_exception;
            if (other._is_exception) {
                new(&_eptr) std::exception_ptr(other._eptr);
            } else {
                new(&_value) T(other._value);
            }
            mark_resolved();
        } else {
            //do nothing
        }
    }
    return *this;
}

template<typename T>
inline async_future<T>& async_future<T>::operator =(const T &value) {
    if (is_ready()) throw async_future_already_resolved();
    new(&_value) T(value);
    mark_resolved();
    return *this;

}

template<typename T>
inline async_future<T>& async_future<T>::operator =(T &&value) {
    if (is_ready()) throw async_future_already_resolved();
    new(&_value) T(std::move(value));
    mark_resolved();
    return *this;
}

template<typename T>
inline async_future<T>& async_future<T>::operator =(std::exception_ptr eptr) {
    if (is_ready()) throw async_future_already_resolved();
    new(&_eptr) std::exception_ptr(eptr);
    mark_resolved();
    return *this;
}

template<typename T>
inline async_future<T>::operator const T&() const {
    if (!is_ready()) throw async_future_not_ready();
    if (_is_exception) std::rethrow_exception(_eptr);
    else return _value;
}

template<typename T>
template<typename Fn>
inline void async_future<T>::addfn1(Fn &&fn) {
    using Callback = async_future_common::Callback<async_future<T>, Fn >;
    if (is_ready()) {
        fn(*this);
    } else {
        auto lt = std::make_unique<Callback>(std::forward<Fn>(fn), nullptr);
        add_cb(lt.release(), false);
    }
}

template<typename T>
template<typename Fn>
inline void async_future<T>::addfn2(Fn &&fn) {
    using Callback2 = async_future_common::Callback2<async_future<T>, Fn >;
    if (is_ready()) {
        fn(*this,false);
    } else {
        auto lt = std::make_unique<Callback2>(std::forward<Fn>(fn), nullptr);
        add_cb(lt.release(), false);
    }
}

template<typename T>
inline bool async_future<T>::is_ready() const {
    return _resolved.load(std::memory_order_acquire);
}


template<typename T>
inline void async_future<T>::mark_resolved() {
    _resolved.store(true, std::memory_order_release);
    flush_cbs(true);
}
template<typename T>
inline void async_future<T>::flush_cbs(bool async) {
    AbstractCallback *l = _callbacks.exchange(nullptr);
    if (l) {
        l->flush_down(*this, async);
        delete l;
    }
}

template<typename T>
inline typename async_future<T>::AbstractCallback* async_future<T>::add_cb(AbstractCallback *itm, bool async) {
    AbstractCallback *nx = itm->next;
    itm->next = nullptr;
    while (!_callbacks.compare_exchange_weak(itm->next, itm)) ;
    if (is_ready()) {
        flush_cbs(async);
    }
    return nx;

}



}
#endif /* ONDRA_SHARED_ASYNC_FUTURE_H_wdj239edj3u30 */

