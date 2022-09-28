/*
 * async_state.h
 *
 *  Created on: 24. 9. 2022
 *      Author: ondra
 */

#ifndef SRC_SHARED_ASYNC_STATE_H_32d0923sjxoaqids123e2
#define SRC_SHARED_ASYNC_STATE_H_32d0923sjxoaqids123e2

#include <algorithm>
#include <atomic>
#include <mutex>
#include "move_only_function.h"

namespace ondra_shared {

template<typename State>
class async_state_t;


namespace async_state_details {
template<typename State>
class holder_t;
}
;

///Declaration of AsyncState pointer
template<typename State>
class async_state_t {
public:

    ///construct async shared state
    template<typename X, typename ... Args>
    static async_state_t<State> make(Args &&... args) {
        return async_state_t(new async_state_details::holder_t<State>(std::forward<Args>(args)...));
    }

    ///construct async shared state
    template<typename X, typename ... Args>
    static async_state_t<State> make(State && st) {
        return async_state_t(new async_state_details::holder_t<State>(std::move(st)));
    }


    
    async_state_t():_ptr(nullptr) {}

    ///Destructor always releases a reference
    /** @note if the reference count reaches the zero, the callback function defined within the
     * state object is called now - in the context of the destructor. If it is not preferred, you
     * can manually release the reference by calling release()
     */
    ~async_state_t() {
        release();
    }

    ///Pointer can be copied
    async_state_t(const async_state_t &other) :
            _ptr(other._ptr) {
        _ptr->addref();
    }
    ///Pointer can be moved (move operation doesn't change reference count)
    async_state_t(async_state_t &&other) :
            _ptr(other._ptr) {
        other._ptr = nullptr;
    }
    ///Pointer can be assigned
    /**
     * @param other other pointer
     * @return reference to this
     *
     * @note because an old value is being released, it can cause, that callback function is called during
     * the assignment operation. It this is not preferred, call release() before assignment
     */
    async_state_t& operator=(const async_state_t &other) {
        if (this != &other) {
            release();
            _ptr = other._ptr;
            _ptr->addref();
        }
        return *this;
    }
    ///Pointer can be moved (move operation doesn't change reference count)
    /**
     * @param other other pointer
     * @return reference to this
     *
     * @note because an old value is being released, it can cause, that callback function is called during
     * the assignment operation. It this is not preferred, call release() before assignment
     */
    async_state_t& operator=(async_state_t &&other) {
        if (this != &other) {
            release();
            _ptr = other._ptr;
            other._ptr = nullptr;
        }
        return *this;
    }

    ///Access the members of the State object
    State* operator->() const {
        return _ptr;
    }
    ///Dereference of the State object
    State& operator*() const {
        return *_ptr;
    }

    ///Lock the state - works only with mutable state
    auto lock() const {
        return _ptr->lock();
    }
    ///Unlock the state - works only with mutable state
    auto unlock() const  {
        return _ptr->unlock();
    }
    ///TryLock the state - works only with mutable state
    auto try_lock() const {
        return _ptr->try_lock();
    }

    ///Release the reference
    /**
     * If the current state pointer is valid, it releases its reference and makes it invalid. It
     * is similar situation as destroying the state pointer. If it is the last reference, the
     * associated callback function is called now, in the context of current thread.
     *
     * If the current state pointer is not valid, nothing is happened. This allows to call release()
     * safety in all situations without need to test, whether the pointer is valid or not
     *
     * @retval true, state has been destroyed, the associated callback function has been called and exited
     * @retval false just released reference or invalid pointer
     */
    bool release() noexcept {
        if (_ptr && _ptr->release()) {
            if (_ptr->_callback)
                _ptr->_callback(*_ptr);
            delete _ptr;
            _ptr = nullptr;
            return true;
        } else {
            _ptr = nullptr;
            return false;
        }
    }

    ///Test whether state is valid
    /**
     * @retval true state is valid
     * @retval false state is not valid - probably released
     */
    bool valid() const {
        return _ptr != nullptr;
    }

    ///Test whether state is valid
    /**
     * @retval false state is valid
     * @retval true state is not valid - probably released
     */
    bool operator!() const {
        return _ptr == nullptr;
    }

    using StateT = typename async_state_details::holder_t<State>::StateT;

    ///Associate a finalizing callback function
    /**
     * @param callback callback function, which receives reference to the State object. Note that
     * even if const is specified with the State object, the callback function receives mutable version
     * - because it is no longer shared so it is safe to modify it. It also allows to callback function to
     * move results without copying
     *
     * @note Function is not MT Safe - only one thread can set the callback
     */
    void onFinish(std::function<void(StateT&)> &&callback) {
        _ptr->_callback = std::move(callback);
    }
    ///Associate a finalizing callback function
    /**
     * @param callback callback function, which receives reference to the State object. Note that
     * even if const is specified with the State object, the callback function receives mutable version
     * - because it is no longer shared so it is safe to modify it. It also allows to callback function to
     * move results without copying
     *
     * @note Function is not MT Safe - only one thread can set the callback
     *
     */
    void operator>>(std::function<void(StateT&)> &&callback) {
        _ptr->_callback = std::move(callback);
    }

protected:
    ///Pointer to state
    async_state_details::holder_t<State> *_ptr;

    template<typename X, typename ... Args>
    friend async_state_t<X> make_async_state(Args &&... args);

    template<typename X>
    friend async_state_t<X> make_async_state(X &&args);

    ///Protected constructor
    async_state_t(async_state_details::holder_t<State> *ptr) :
            _ptr(ptr) {
        _ptr->addref();
    }
};

namespace async_state_details {

///Declaration of state holder for const State
template<typename State>
class holder_t<const State> : public State {
public:

    using StateT = State;

    template<typename ... Args>
    holder_t(Args &&... args) :
            State(std::forward<Args>(args)...), _shareCount(0) {
    }
    holder_t(const holder_t &other) = delete;
    holder_t& operator=(const holder_t &other) = delete;
protected:
    friend class async_state_t<const State> ;

    std::atomic<unsigned int> _shareCount;
    std::function<void(State&)> _callback;

    void addref() {
        //no ordering is enforced as new reference doesn't need anything from other threads
        _shareCount.fetch_add(1, std::memory_order_relaxed);
    }
    bool release() {
        //when reference is released, ensure that all writes are done before acquire
        if (_shareCount.fetch_sub(1, std::memory_order_release) - 1 == 0) {
            //before finish operation is started, acquire all writes (from othe threads made before release)
            _shareCount.load(std::memory_order_acquire);
            return true;
        } else {
            return false;
        }
    }

    void lock() {
        //empty - as there is no lock, locking is not necessary
    }
    void unlock() {
        //empty - as there is no lock, locking is not necessary
    }
    bool try_lock() {
        //empty - as there is no lock, locking is not necessary
        return true;
    }
};

///Declaration of state holder for mutable State
template<typename State>
class holder_t: public holder_t<const State> {
protected:

    using StateT = State;

    friend class async_state_t<State> ;

    using holder_t<const State>::holder_t;

    std::mutex _mx;

    void lock() {
        _mx.lock();
    }
    void unlock() {
        _mx.unlock();
    }
    bool try_lock() {
        return _mx.try_lock();
    }
};

}

}



#endif /* SRC_SHARED_ASYNC_STATE_H_32d0923sjxoaqids123e2 */
