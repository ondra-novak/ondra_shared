#ifndef _ONDRA_SHARED_ISCALLABLE_231294921928319
#define _ONDRA_SHARED_ISCALLABLE_231294921928319

namespace ondra_shared {

namespace _details {
     template<typename T> struct IsVoid {typedef std::false_type Value;};
     template<> struct IsVoid<void> {typedef std::true_type Value;};
}


///Determines, whether function Q is callable using specified prototype T
/**
 * @tparam Prototype specify prototype of function, for example void(int), the
 * same way how std::function is declared
 *
 * @tparam Function specify type of function to call. If you don't know type, use
 * declspec(function)
 *
 * @return Template class returns value in type Value
 * @retval std::true_type function is callable with given prototype
 * @retval std::false_type function is not callable by given prototype
 *
 *
 * @note return value of the function is also checked unless you specify "void"
 * in the prototype. With void, return type is ignored
 */



template<typename T, typename Q>
class IsCallable;

template<typename Ret, typename... Args, typename Q>
class IsCallable<Ret(Args...),Q> {
public:

     class NotDefinedType {};

     template<typename Z>
     static auto trycall(Z &&, Args&&... args) -> decltype(std::declval<Z>()(args...));
     static NotDefinedType trycall(...);

     static std::true_type checkRes(Ret);
     static std::false_type checkRes(...);

     typedef decltype(checkRes(trycall(std::declval<Q>(),std::declval<Args>()...))) Value;

     static const bool value = Value::value;

};

template<typename X> class PrintX;

template< typename... Args, typename Q>
class IsCallable<void(Args...),Q> {
public:

     class NotDefinedType {};

     template<typename Z>
     static auto trycall(Z &&, Args&&... args) -> decltype(std::declval<Z>()(args...));
     static NotDefinedType trycall(...);

     static std::true_type checkRes(std::false_type);
     static std::false_type checkRes(std::true_type);

     typedef decltype(trycall(std::declval<Q>(),std::declval<Args>()...)) CallRes;

     typedef typename std::is_same<std::false_type, typename std::is_same<CallRes,NotDefinedType>::type >::type Value;

     static const bool value = Value::value;
};


}

#endif
