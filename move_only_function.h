/*
 * move_only_function.h
 *
 *  Created on: 23. 6. 2022
 *      Author: ondra
 */

#ifndef SRC_LIBS_SHARED_MOVE_ONLY_FUNCTION_H_
#define SRC_LIBS_SHARED_MOVE_ONLY_FUNCTION_H_

#include <type_traits>

namespace ondra_shared {

namespace _details {
    ///Converts T => const T &
    template<typename Type> struct FastParamT {using T = const Type &;};
    ///Converts T& => T&
    template<typename Type> struct FastParamT<Type &> {using T = Type &;};
    ///Converts T&& => T&&
    template<typename Type> struct FastParamT<Type &&> {using T = Type &&;};
    ///Converts T * => T *
    template<typename Type> struct FastParamT<Type *> {using T = Type *;};
    ///Converts const T * => const T *
    template<typename Type> struct FastParamT<const Type *> {using T = const Type *;};
    template<typename T> using FastParam = typename FastParamT<T>::T;

    class move_only_function_details {
    public:
        template<typename T>
        using FP = FastParam<T>;
    };
}


template<typename T> class move_only_function;

template<typename R, typename ... Args, bool nx>
class move_only_function<R(Args...) noexcept(nx)>: public _details::move_only_function_details {

    class AbstractFn {
    public:
        virtual R call(FP<Args>... args) noexcept(nx) = 0;
    };

    template<typename Fn>
    class CallFn: public AbstractFn {
    public:
        virtual R call(FP<Args>... args) noexcept(nx) override  {
            return R(fn(std::forward<FP<Args> >(args)...));
        }
        CallFn(Fn &&fn):fn(std::forward<Fn>(fn)) {}
    protected:
        Fn fn;
    };

public:

    move_only_function() = default;

    template<typename Fn>
    move_only_function(Fn &&fn)
        :_ptr(std::make_unique<CallFn<Fn> >(std::forward<Fn>(fn))) {}
    move_only_function(std::nullptr_t) {};

    move_only_function(move_only_function &&other) = default;
    move_only_function &operator=(move_only_function &&other) = default;

    move_only_function(const move_only_function &other) = delete;
    move_only_function(const move_only_function &&other) = delete;
    move_only_function(move_only_function &other) = delete;
    move_only_function &operator=(const move_only_function &other) = delete;
    move_only_function &operator=(const move_only_function &&other) = delete;
    move_only_function &operator=(move_only_function &other) = delete;

    operator bool() const {return _ptr != nullptr;}
    bool operator==(std::nullptr_t) const {return _ptr == nullptr;}
    bool operator!=(std::nullptr_t) const {return _ptr != nullptr;}

    R operator()(FP<Args>... args) const noexcept(nx) {
        return _ptr->call(std::forward<FP<Args>>(args)...);
    }
private:
    std::unique_ptr<AbstractFn> _ptr;
};




}


#endif /* SRC_LIBS_SHARED_MOVE_ONLY_FUNCTION_H_ */
