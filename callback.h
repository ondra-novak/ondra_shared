/*
 * callback.h
 *
 *  Created on: 27. 5. 2022
 *      Author: ondra
 */

#ifndef SRC_ONDRA_SHARED_CALLBACK_H_78946548694984654
#define SRC_ONDRA_SHARED_CALLBACK_H_78946548694984654

#include <memory>
#include <typeinfo>

#include "fastsharedalloc.h"

namespace ondra_shared {



///Callback class - wraps function or lambda to abstract callable object
/**
 * Works similar as std::function - but cannot be copied, however it allow movable
 * closures (in contrast to std::function). And it is very lightweight
 *
 * To use template, declare function prototype as T (example T=void(Arg1,Arg2,...) )
 *
 * This template contains useful feature: You can pass called instance as parameter of
 * the function
 *
 * @code
 * Callback<void(int)> fn = [&](Callback<void(int)> &me, int param) {...}
 * @endcode
 *
 * In the example above, argument 'me' contains reference to called instance. This allows
 * to call function recursively. You can even move the pointer to the next call (for
 * example when function is used as completion function
 */
template<typename T> class Callback;


///Converts Type to its const Type reference variant if it is possible.
template<typename Type> struct FastParamT {using T = const Type &;};
template<typename Type> struct FastParamT<Type &> {using T = Type &;};
template<typename Type> struct FastParamT<Type &&> {using T = Type &&;};
template<typename Type> struct FastParamT<Type *> {using T = Type *;};
template<typename Type> struct FastParamT<const Type *> {using T = const Type *;};
template<typename T> using FastParam = typename FastParamT<T>::T;



template<typename R, typename ... Args>
class Callback<R(Args...)> {
public:

    ///Construct empty callback object. You can assign later
    Callback() {}
    ///Assign nullptr to callback creates unitialized callback
    Callback(std::nullptr_t) {}
    ///Object is not copieable
    Callback(const Callback &other) = delete;
    ///Delete this type of constructor - it is needed to use std::move()
    Callback(Callback &other) = delete;
    ///Delete this type of constructor
    Callback(const Callback &&other) = delete;
    ///You can move object
    Callback(Callback &&other);
    ///You can assign by move
    Callback &operator=(Callback &&other);
    ///You can't assign by copy
    Callback &operator=(const Callback &other) = delete;
    ///You can't assign by copy
    Callback &operator=(const Callback &&other) = delete;
    ///You can't assign by copy
    Callback &operator=(Callback &other) = delete;
    ///Construct with a function
    template<typename Fn> Callback(Fn &&fn);
    ///Construct with pointer to an object and pointer to a member function
    /**
     * @param ptr pointer to object. It is not limited to raw pointer. It can be a smart pointer as well
     * @param fn pointer to member function of the object
     */
    template<typename Ptr, typename Obj, typename ... MArgs, typename = decltype((std::declval<Ptr>()->*(std::declval<R (Obj::*)(MArgs ...)>()))(std::declval<Args>()...))>
    Callback(Ptr &&ptr, R (Obj::*fn)(MArgs ... args));

    ///Call the function
    /**
     * @param args function arguments
     * @return function result
     */
    R operator()(FastParam<Args>  ... args) const;

    ///Test whether function is assigned
    bool operator==(std::nullptr_t) const {return ptr == nullptr;}
    ///Test whether function is assigned
    bool operator!=(std::nullptr_t) const {return ptr != nullptr;}

    ///Clear assigned function
    void reset() {ptr = nullptr;}

protected:

    class AbstractCall:public FastSharedAlloc {
    public:
        virtual R call(Callback &me, FastParam<Args>  ... args)  = 0;
        virtual ~AbstractCall() {}
    };

    using Ptr_t = std::unique_ptr<AbstractCall>;

    Ptr_t ptr;


    template<typename Fn>
    class FnCall: public AbstractCall {
    public:
        FnCall(Fn &&fn):fn(std::forward<Fn>(fn)) {}

        virtual R call(Callback &, FastParam<Args>  ... args) override {
            return fn(std::forward<FastParam<Args> >(args)...);
        }

    protected:
        Fn fn;
    };

    template<typename Fn>
    class FnCallWithMe: public AbstractCall {
    public:
        FnCallWithMe(Fn &&fn):fn(std::forward<Fn>(fn)) {}

        virtual R call(Callback &me, FastParam<Args>  ... args) override {
            return fn(me, args...);
        }

    protected:
        Fn fn;
    };


    template<typename Fn>
    static auto make(Fn &&fn)
        -> decltype(std::declval<Fn>()(std::declval<Args>()...),Ptr_t()) {
        return std::make_unique<FnCall<Fn> >(std::forward<Fn>(fn));
    }
    template<typename Fn>
    static auto make(Fn &&fn)
        -> decltype(std::declval<Fn>()(std::declval<Callback &>(), std::declval<Args>()...),Ptr_t()) {
        return std::make_unique<FnCallWithMe<Fn> >(std::forward<Fn>(fn));
    }
};

template<typename R, typename ... Args>
Callback<R(Args...)>::Callback(Callback &&other)
    :ptr(std::move(other.ptr)) {}

template<typename R, typename ... Args>
template<typename Ptr, typename Obj, typename ... MArgs, typename >
Callback<R(Args...)>::Callback(Ptr &&ptr, R (Obj::*fn)(MArgs ... args))
:Callback([ptr = Ptr(std::forward<Ptr>(ptr)),fn](FastParam<Args> ... args) {
    (ptr->*fn)(args...);
})
{}


template<typename R, typename ... Args>
Callback<R(Args...)> &Callback<R(Args...)>::operator=(Callback &&other) {
    if (this != &other) {
        ptr = std::move(other.ptr);
    }
    return *this;
}

template<typename R, typename ... Args>
template<typename Fn>
Callback<R(Args...)>::Callback(Fn &&fn)
    :ptr(make(std::forward<Fn>(fn)))
{}

template<typename R, typename ... Args>
R Callback<R(Args...)>::operator()(FastParam<Args> ... args) const {
    if (ptr) {
        return ptr->call(*const_cast<Callback *>(this), std::forward<FastParam<Args> >(args)...);
    } else {
        throw std::runtime_error(std::string("Attempt to call unassigned callback: ").append(typeid(Callback).name()));
    }
}

}

#endif
