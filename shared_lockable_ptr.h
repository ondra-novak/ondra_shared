/**
 * @file introduces a smart pointer, which must be locked to access the data
 *
 * The smart pointer works similar as shared_ptr, but if you need to access the data, you
 * have to call either lock() or lock_shared() depend on which operation you want
 *
 * Attempt to lock const object results to lock shared always.
 *
 * By locking the smart pointer, you receive another smart pointer, which acts as unique_ptr, so
 * it can be moved, but not copied. You also cannot assign to it. It allows you to
 * access the underlying object. During the lifetime of this pointer, the lock is held, until
 * this smart pointer is destroyed.
 *
 * Complexity - each smart pointer occupies space for two raw pointers. There is also
 * extra memory allocation for every shared object. When you lock the smart pointer, you
 * receive object with additional two raw pointers.
 *
 *
 * To create shared_lockable_ptr, you can call make_shared_lockable() for convince. You can also
 * create the instance from existing raw pointer. You can supply own deleter. You can freely
 * convert pointer to parent pointer, const pointer, whatever is possible by standard pointer
 * conversion.
 *
 * If you need to create shared_lockable_ptr from this, your object need to inherit
 * the class enable_share_lockable_from_this<T>. This allows you to call share_from_this() to
 * obtain shared_lockable_ptr<T> to this object. Note that you should avoid to do this in
 * destructors.
 *
 *
 */

#include <atomic>
#include <shared_mutex>


#ifndef __ONDRA_SHARED_SHARED_LOCKABLE_PTR_H_54JUIPOJOI32
#define __ONDRA_SHARED_SHARED_LOCKABLE_PTR_H_54JUIPOJOI32

namespace ondra_shared {

template<typename T> class shared_lockable_ptr;

template<typename T, typename Q>
shared_lockable_ptr<T> const_pointer_cast(const shared_lockable_ptr<Q> &ptr);
template<typename T, typename Q>
shared_lockable_ptr<T> static_pointer_cast(const shared_lockable_ptr<Q> &ptr);
template<typename T, typename Q>
shared_lockable_ptr<T> dynamic_pointer_cast(const shared_lockable_ptr<Q> &ptr);



namespace shared_lockable_ptr_details {

    class abstract_control {
    public:
        abstract_control():_refs(0) {}
        abstract_control(const abstract_control &other) = delete;
        abstract_control &operator=(const abstract_control &other) = delete;

        virtual void destroy() noexcept = 0;
        virtual ~abstract_control() = default;

        void lock() {_mx.lock();}
        void unlock() {_mx.unlock();}
        void lock_shared() {_mx.lock_shared();}
        void unlock_shared() {_mx.unlock_shared();}

        void add_ref() {
            ++_refs;
        }

        void release_ref() {
            if (--_refs == 0) {
                //increase reference counter to avoid  reentrace by mistake during destruction
                ++_refs;
                //start destruction
                destroy();
                //after destruction, delete me
                delete this;
            }
        }

    protected:
        std::atomic<std::size_t> _refs;
        std::shared_timed_mutex _mx;
    };

    template<typename T>
    class abstract_control_with_type: public abstract_control {
    public:
        virtual T *get_ptr() const = 0;
    };


    template<typename T, typename Deleter>
    class control_with_deleter: public abstract_control_with_type<T> {
    public:
        control_with_deleter(T *ptr, Deleter &&deleter)
            :_ptr(ptr)
            ,deleter(std::forward<Deleter>(deleter)) {}

        virtual void destroy() noexcept override {
            deleter(_ptr);
        }

        virtual T *get_ptr() const override{
            return _ptr;
        }
    protected:
        T *_ptr;
        Deleter deleter;
    };

    template<typename T>
    struct default_deleter {
        void operator()(T *x) const {
            delete x;
        }
    };

    template<typename T>
    using control = control_with_deleter<T, default_deleter<T> >;

    template<typename T>
    class control_with_object: public abstract_control_with_type<T> {
    public:
        template<typename ... Args>
        control_with_object(Args && ... args)
            :_ptr(reinterpret_cast<T *>(buffer)) {
            new(this->_ptr) T(std::forward<Args>(args)...);
        }

        virtual void destroy() noexcept override {
            _ptr->~T();
        }

        virtual T *get_ptr() const override {
            return _ptr;
        }
    protected:
        static constexpr std::size_t req_size = (sizeof(T)+sizeof(std::size_t)-1)/sizeof(std::size_t);
        T *_ptr;
        std::size_t buffer[req_size];
    };


    template<typename T, class Locker>
    class locked_pointer {
    public:
        locked_pointer():_cntr(nullptr),_ptr(nullptr) {}
        locked_pointer(T *ptr, abstract_control *cntr):_cntr(cntr),_ptr(ptr) {
            _cntr->add_ref();
            Locker::lock(_cntr);
        }
        ~locked_pointer() {
            if (_cntr) {
                Locker::unlock(_cntr);
                _cntr->release_ref();
            }
        }

        locked_pointer(const locked_pointer &other) = delete;
        locked_pointer(locked_pointer &&other):_cntr(other._cntr),_ptr(other._ptr) {
            other._cntr = nullptr;
            other._ptr = nullptr;
        }

        locked_pointer &operator=(const locked_pointer &other) = delete;
        locked_pointer &operator=(locked_pointer &&other) = delete;

        T *operator->() const {return _ptr;}
        T &operator *() const {return *_ptr;}
        T *get() const  {return *_ptr;}
        operator T *() const {return *_ptr;}

        template<typename L>
        bool operator==(const locked_pointer<T, L> &other) {return _ptr == other._ptr;}
        template<typename L>
        bool operator!=(const locked_pointer<T, L> &other) {return _ptr != other._ptr;}

        void release() {
            if (_cntr) {
                Locker::unlock(_cntr);
                _cntr->release_ref();
                _cntr = nullptr;
                _ptr = nullptr;
            }
        }

    protected:
        mutable abstract_control *_cntr;
        T *_ptr;
    };


    template<typename T>
    class shared_locker {
    public:
        using TargetT = const T;

        static void lock(abstract_control *cntr) {
            cntr->lock_shared();
        }
        static void unlock(abstract_control *cntr) {
            cntr->unlock_shared();
        }
    };

    template<typename T>
    class exclusive_locker {
    public:
        using TargetT = T;

        static void lock(abstract_control *cntr) {
            cntr->lock();
        }
        static void unlock(abstract_control *cntr) {
            cntr->unlock();
        }
    };



    template<typename T>
    class exclusive_locker<const T> {
    public:
        using TargetT = const T;
        static void lock(abstract_control *cntr) {
            cntr->lock_shared();
        }
        static void unlock(abstract_control *cntr) {
            cntr->unlock_shared();
        }
    };

    template<typename T> class smart_ptr;
    template<typename T> class control_static_alloc;



    template<typename T>
    abstract_control_with_type<T> *set_enable_share_lockable_from_this(abstract_control_with_type<T> *cntr, ...) {
        return cntr;
    }

    template<typename T>
    class control_static_alloc: public abstract_control_with_type<T> {
    public:
        control_static_alloc(T *ptr)
            :_ptr(ptr) {}
        T *get_ptr() const override {
            return _ptr;
        }

        void destroy() noexcept override {
            //we do not destroy the class, we just set it not-shared
            //because it must be deleted separatedly
            _ptr->_cntr = nullptr;
        }

    protected:
        T *_ptr;
    };

}

///Introduces shared ptr with shared/exclusive lock
/**
 * This allows to share MT unsafe objects without extra code change. The MT
 * safety is handled by the smart pointer itself
 * @tparam T type which is wrapped into the pointer
 *
 * @note you can freely convert type to any compatible type anytime later, which
 * still maintains the MT safety through its interface.
 *
 * @note about consts. To create const version of the shared object,
 * use shared_lockable_ptr<const T> not const shared_lockable_ptr<T>. The
 * second one makes just const pointer, not content on which it points.
 *
 * @note about mutable keyword. If used in protected class, remember, that
 * const object is always locked as shared, so writes in a const method are
 * not MT safe in this case. It is better to use remove_const method (which is similar
 * to const_cast)
 */
template<typename T>
class shared_lockable_ptr {
public:

    using shared_locker = shared_lockable_ptr_details::shared_locker<T>;

    using exclusive_locker = shared_lockable_ptr_details::exclusive_locker<T>;
    ///Abstract control structure
    using abstract_control = shared_lockable_ptr_details::abstract_control;
    ///locked smart pointer
    /** @note for const T variant, there is no exclusive locking. All types of locking
     * ends by shared locking
     */
    using locked = shared_lockable_ptr_details::locked_pointer<typename exclusive_locker::TargetT, exclusive_locker >;
    ///shared locked pointer
    using locked_shared = shared_lockable_ptr_details::locked_pointer<typename shared_locker::TargetT, exclusive_locker >;

    ///Construct empty pointer
    shared_lockable_ptr()
        :_ptr(nullptr)
        ,_cntr(nullptr) {}
    ///Construct empty pointer
    shared_lockable_ptr(std::nullptr_t)
        :_ptr(nullptr)
        ,_cntr(nullptr) {}
    ///Construct shared_lockable_ptr from a raw pointer
    shared_lockable_ptr(T *ptr)
        :_ptr(ptr)
        ,_cntr(shared_lockable_ptr_details::set_enable_share_lockable_from_this(
                new shared_lockable_ptr_details::control<T>(ptr),ptr)) {
        _cntr->add_ref();
    }
    ///Construct shared_lockable_ptr from already existing control object
    shared_lockable_ptr(shared_lockable_ptr_details::abstract_control_with_type<T> *cntr)
        :_ptr(cntr->get_ptr())
        ,_cntr(cntr) {
        _cntr->add_ref();
    }

    ///Construct shared_lockable_ptr from raw pointer, you can specify a deleter function
    template<typename Deleter>
    shared_lockable_ptr(T *ptr, Deleter &&deleter)
        :_ptr(ptr)
        ,_cntr(shared_lockable_ptr_details::set_enable_share_lockable_from_this(
                new shared_lockable_ptr_details::control_with_deleter<T, Deleter>(ptr, std::forward<Deleter>(deleter))),ptr) {
        _cntr->add_ref();
    }

    ///Copy the pointer
    shared_lockable_ptr(const shared_lockable_ptr &other)
        :_ptr(other._ptr)
        ,_cntr(other._cntr) {
        if (_cntr) _cntr->add_ref();
    }

    ///Move the pointer
    shared_lockable_ptr(shared_lockable_ptr &&other)
        :_ptr(other._ptr)
        ,_cntr(other._cntr) {
        other._ptr = nullptr;
        other._cntr = nullptr;
    }

    ///Convert the pointer to the different type
    template<typename Q, typename = decltype(std::declval<T * &>() = std::declval<Q *>())>
    shared_lockable_ptr(const shared_lockable_ptr<Q> &other)
        :_ptr(other._ptr)
        ,_cntr(other._cntr) {
        if (_cntr) _cntr->add_ref();
    }

    ///Assign to the pointer
    shared_lockable_ptr &operator=(const shared_lockable_ptr &other) {
        if (&other != this) {
            auto old_cntr = _cntr;
            _ptr = other._ptr;
            _cntr = other._cntr;
            if (_cntr) _cntr->add_ref();
            if (old_cntr) old_cntr->release_ref();
        }
        return *this;
    }

    ///Assign to the pointer using rvalue reference
    shared_lockable_ptr &operator=(shared_lockable_ptr &&other) {
        if (&other != this) {
            if (_cntr) _cntr->release_ref();
            _ptr = other._ptr;
            _cntr = other._cntr;
            other._cntr = nullptr;
            other._ptr =nullptr;
        }
        return *this;
    }

    ///Assign and convert
    template<typename Q, typename = decltype(std::declval<T * &>() = std::declval<Q *>())>
    shared_lockable_ptr &operator=(const shared_lockable_ptr<Q> &other) {
        if (static_cast<const void *>(&other) != static_cast<const void *>(this)) {
            auto old_cntr = _cntr;
            _ptr = other._ptr;
            _cntr = other._cntr;
            if (_cntr) _cntr->add_ref();
            if (old_cntr) old_cntr->release_ref();
        }
        return *this;
    }

    ///Returns true, if  pointer shares anything
    operator bool() const {return _ptr != nullptr;}
    ///Returns true, if  pointer doesn't share anything
    bool operator !() const {return _ptr == nullptr;}

    ///compare with null
    bool operator==(std::nullptr_t) const {return _ptr == nullptr;}
    ///compare with null
    bool operator!=(std::nullptr_t) const {return _ptr != nullptr;}

    template<typename Q>
    bool operator==(const shared_lockable_ptr<Q> &other) const {return _ptr == other._ptr;}
    template<typename Q>
    bool operator!=(const shared_lockable_ptr<Q> &other) const {return _ptr != other._ptr;}
    template<typename Q>
    bool operator>=(const shared_lockable_ptr<Q> &other) const {return _ptr >= other._ptr;}
    template<typename Q>
    bool operator<=(const shared_lockable_ptr<Q> &other) const {return _ptr <= other._ptr;}
    template<typename Q>
    bool operator>(const shared_lockable_ptr<Q> &other) const {return _ptr > other._ptr;}
    template<typename Q>
    bool operator<(const shared_lockable_ptr<Q> &other) const {return _ptr < other._ptr;}


    ///Lock the object and receive exclusive access
    /**
     * @return smart pointer - movable only - which can be used to access the object.
     * If you no longer need it, you need to destroy it, otherwise the lock is
     * kept for whole lifetime of this object
     *
     * @note For const reference, you always get shared lock
     */
    locked lock() const {
        return locked(_ptr, _cntr);
    }
    ///Lock the object and receive shared access
    /**
     *
     * @return
     */
    locked_shared lock_shared() const {
        return locked_shared(_ptr, _cntr);
    }


    template<typename ... Args>
    static shared_lockable_ptr<T> make(Args && ... args) {
        using namespace shared_lockable_ptr_details;
       abstract_control_with_type<T> *cntr = new control_with_object<T>(std::forward<Args>(args)...);
        return shared_lockable_ptr<T>(set_enable_share_lockable_from_this(cntr, cntr->get_ptr()));
    }

    shared_lockable_ptr<const T> get_const() const {
        return shared_lockable_ptr<const T>(*this);
    }

    shared_lockable_ptr<std::remove_const<T> > remove_const() const {
        return shared_lockable_ptr<T>(const_cast<T *>(_ptr),_cntr);
    }

    template<typename Q> friend class shared_lockable_ptr;

    template<typename T1,typename Q>
    friend shared_lockable_ptr<T1> const_pointer_cast(const shared_lockable_ptr<Q> &ptr);
    template<typename T1,typename Q>
    friend shared_lockable_ptr<T1> static_pointer_cast(const shared_lockable_ptr<Q> &ptr);
    template<typename T1,typename Q>
    friend shared_lockable_ptr<T1> dynamic_pointer_cast(const shared_lockable_ptr<Q> &ptr);


    ~shared_lockable_ptr() {
        if (_cntr) _cntr->release_ref();
    }
protected:
    T *_ptr;
    abstract_control *_cntr;


    shared_lockable_ptr(T *ptr, abstract_control *cntr)
        :_ptr(ptr)
        ,_cntr(cntr) {
            _cntr->add_ref();
    }
};


template<typename T>
class enable_share_lockable_from_this {
public:
    enable_share_lockable_from_this() = default;
    enable_share_lockable_from_this(const enable_share_lockable_from_this &other) {};
    enable_share_lockable_from_this &operator=(const enable_share_lockable_from_this &other) {return *this;};

    shared_lockable_ptr<T> share_from_this();
    shared_lockable_ptr<const T> share_from_this() const;

    friend shared_lockable_ptr_details::abstract_control_with_type<T> *set_control_object(
                    enable_share_lockable_from_this *me,
                    shared_lockable_ptr_details::abstract_control_with_type<T> *cntr) {
        shared_lockable_ptr_details::abstract_control_with_type<T> *c = nullptr;
        if (!me->_cntr.compare_exchange_strong(c, cntr)) {
            delete cntr;
            return c;
        } else {
            return cntr;
        }
    }

    template<typename Q> friend class shared_lockable_ptr_details::control_static_alloc;


private:
    mutable std::atomic<shared_lockable_ptr_details::abstract_control_with_type<T> *> _cntr = nullptr;

};

template<typename T>
  shared_lockable_ptr_details::abstract_control_with_type<T> *
        shared_lockable_ptr_details::set_enable_share_lockable_from_this(
                    abstract_control_with_type<T> *cntr,
                    enable_share_lockable_from_this<T> *obj) {
    return set_control_object(obj, cntr);
  }

template<typename T>
inline shared_lockable_ptr<T> enable_share_lockable_from_this<T>::share_from_this()  {
    if (_cntr == nullptr) {
        //not shared or statically allocated - but we can mimic the shared object
        _cntr = new shared_lockable_ptr_details::control_static_alloc<T>(static_cast<T *>(this));
    }
    return shared_lockable_ptr<T>(_cntr);
}
template<typename T>
inline shared_lockable_ptr<const T> enable_share_lockable_from_this<T>::share_from_this() const {
    if (_cntr == nullptr) {
        //not shared or statically allocated - but we can mimic the shared object
        _cntr = new shared_lockable_ptr_details::control_static_alloc<T>(const_cast<T *>(static_cast<const T *>(this)));
    }
    return shared_lockable_ptr<T>(_cntr);
}


template<typename T, typename Q>
shared_lockable_ptr<T> const_pointer_cast(const shared_lockable_ptr<Q> &ptr) {
    return shared_lockable_ptr<T>(const_cast<T *>(ptr._ptr), ptr._cntr);
}
template<typename T, typename Q>
shared_lockable_ptr<T> static_pointer_cast(const shared_lockable_ptr<Q> &ptr) {
    return shared_lockable_ptr<T>(static_cast<T *>(ptr._ptr), ptr._cntr);
}
template<typename T, typename Q>
shared_lockable_ptr<T> dynamic_pointer_cast(const shared_lockable_ptr<Q> &ptr) {
    return shared_lockable_ptr<T>(dynamic_cast<T *>(ptr._ptr), ptr._cntr);
}


template<typename T, typename ... Args>
shared_lockable_ptr<T> make_shared_lockable(Args && ... args) {
    return shared_lockable_ptr<T>::make(std::forward<Args>(args)...);
}


}
#endif /* __ONDRA_SHARED_SHARED_LOCKABLE_PTR_H_54JUIPOJOI32 */
