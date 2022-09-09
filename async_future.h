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
        virtual void run(const Me &) noexcept = 0;
        virtual ~AbstractCallback() {
            if (next) delete next;
        }
        void flush_down(const Me &x) {
            run(x);
            if (next) next->flush_down(x);
        }
    };

    template<typename Me, typename Fn>
    class Callback: public AbstractCallback<Me> {
    public:
        Callback(Fn &&fn, AbstractCallback<Me> *next)
            :AbstractCallback<Me>(next), fn(std::forward<Fn>(fn)) {}
        virtual void run(const Me &x) noexcept {
            fn(x);
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

    ///Attach new callback
    /**
     * @param cb callback function, which accepts the const reference to the future object
     *
     * @note if the future is already resolved, the callback function is immediatelly called,
     * otherwise it is registered and can be called when the future is resolved.
     *
     * The callback function receives reference to this future. The callback function should
     * not assume, that future is always ready. If the future is destroyed before
     * it is resolved, the callback function is called with the not ready yet future (meaning
     * that future will be never resolved)
     */
    template<typename Fn, typename = decltype(std::declval<Fn>()(std::declval<const async_future<T> &>()))>
    void operator>>(Fn &&cb);

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
    void flush_cbs();
    AbstractCallback *add_cb(AbstractCallback *itm);

};

template<>
class async_future<void>: public async_future_common {
public:


    async_future();
    async_future(async_future &&other);
    async_future(const async_future &other);
    async_future(bool ready);
    async_future(std::exception_ptr eptr);
    ~async_future();

    async_future &operator=(async_future &&other);
    async_future &operator=(const async_future &other);
    async_future &operator=(bool ready);
    async_future &operator=(std::exception_ptr eptr);

    operator bool() const;

    template<typename Fn, typename = decltype(std::declval<Fn>()(std::declval<const async_future<void> &>()))>
    void operator>>(Fn &&cb);

    bool is_ready() const;
    bool operator !() const {return !is_ready();}



protected:

    using AbstractCallback = async_future_common::AbstractCallback<async_future<void> >;


    std::atomic<bool> _resolved;
    std::atomic<AbstractCallback *> _callbacks;
    std::exception_ptr _eptr;

    void mark_resolved();
    void flush_cbs();
    AbstractCallback *add_cb(AbstractCallback *itm);

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
,_callbacks(other._callbacks)
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
        z->flush_down(*this);
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
template<typename Fn, typename>
inline void async_future<T>::operator >>(Fn &&fn) {
    using Callback = async_future_common::Callback<async_future<T>, Fn >;
    if (is_ready()) {
        fn(*this);
    } else {
        auto lt = std::make_unique<Callback>(std::forward<Fn>(fn), nullptr);
        add_cb(lt.release());
    }
}

template<typename T>
inline bool async_future<T>::is_ready() const {
    return _resolved.load(std::memory_order_acquire);
}


template<typename T>
inline void async_future<T>::mark_resolved() {
    _resolved.store(true, std::memory_order_release);
    flush_cbs();
}
template<typename T>
inline void async_future<T>::flush_cbs() {
    AbstractCallback *l = _callbacks.exchange(nullptr);
    if (l) {
        l->flush_down(*this);
        delete l;
    }
}

template<typename T>
inline typename async_future<T>::AbstractCallback* async_future<T>::add_cb(AbstractCallback *itm) {
    AbstractCallback *nx = itm->next;
    itm->next = nullptr;
    while (!_callbacks.compare_exchange_weak(itm->next, itm)) ;
    if (is_ready()) {
        flush_cbs();
    }
    return nx;

}


inline async_future<void>::async_future()
:_resolved(false),_callbacks(nullptr), _eptr(nullptr) {}

inline async_future<void>::async_future(async_future<void>&& other)
:_resolved(other.is_ready()),_callbacks(other._callbacks.exchange(nullptr)),_eptr(std::move(other._eptr))
{
}

inline async_future<void>::async_future(const async_future& other)
:_resolved(other.is_ready()),_callbacks(nullptr),_eptr(other._eptr)
{
}

inline async_future<void>::async_future(bool ready)
:_resolved(ready),_callbacks(nullptr),_eptr(nullptr)
{
}

inline async_future<void>::async_future(std::exception_ptr eptr)
:_resolved(true),_callbacks(nullptr),_eptr(eptr)
{
}

inline async_future<void>::~async_future()
{
    auto l = _callbacks.exchange(nullptr);
    if (l != nullptr) {
        l->flush_down(*this);
        delete l;
    }
}

inline async_future<void>& async_future<void>::operator =(async_future<void> && other)
{
    if (this != &other) {
        if (is_ready()) throw async_future_already_resolved();
        if (other.is_ready()) {
            mark_resolved();
        } else {
            auto z = other._callbacks.exchange(nullptr);
            while (z) z = add_cb(z);
        }
    }
    return *this;
}

inline async_future<void>& async_future<void>::operator =(const async_future<void>& other)
{
    if (this != &other) {
        if (is_ready()) throw async_future_already_resolved();
        if (other.is_ready()) {
            mark_resolved();
        }
    }
    return *this;
}

inline async_future<void>& async_future<void>::operator =(bool ready) {
    if (this->is_ready()) throw async_future_already_resolved();
    if (ready) mark_resolved();
    return *this;
}


inline async_future<void>& async_future<void>::operator =(std::exception_ptr eptr)
{
    if (is_ready()) throw async_future_already_resolved();
    _eptr = eptr;
    mark_resolved();
    return *this;
}

inline async_future<void>::operator bool() const
{
    return is_ready();
}

template<typename Fn, typename = decltype(std::declval<Fn>()(std::declval<const async_future<void> &>()))>
inline void async_future<void>::operator >>(Fn&& cb)
{
    using Callback = async_future_common::Callback<async_future<void>, Fn >;
    if (is_ready()) {
        cb(*this);
    } else {
        auto lt = std::make_unique<Callback>(std::forward<Fn>(cb), nullptr);
        add_cb(lt.release());
    }

}

inline bool async_future<void>::is_ready() const
{
    return _resolved.load(std::memory_order_acquire);
}

inline void async_future<void>::mark_resolved()
{
    _resolved.store(true, std::memory_order_release);
    flush_cbs();
}

inline void async_future<void>::flush_cbs()
{
    auto z = _callbacks.exchange(nullptr);
    if (z) {
        z->flush_down(*this);
        delete z;
    }
}

inline async_future<void>::AbstractCallback* async_future<void>::add_cb(AbstractCallback* itm)
{
    AbstractCallback *nx = itm->next;
    itm->next = nullptr;
    while (!_callbacks.compare_exchange_weak(itm->next, itm)) ;
    if (is_ready()) {
        flush_cbs();
    }
    return nx;
}

}
#endif /* ONDRA_SHARED_ASYNC_FUTURE_H_wdj239edj3u30 */

