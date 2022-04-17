/*
 * shared_function.h
 *
 *  Created on: 12. 10. 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_H_SHARED_FUNCTION_H_23232idewdkwp3dksmc
#define ONDRA_SHARED_H_SHARED_FUNCTION_H_23232idewdkwp3dksmc

#include <memory>
#include <type_traits>
#include <functional>
#include <tuple>
#include "refcnt.h"
#include "apply.h"

namespace ondra_shared {




namespace _details {
	template<typename T> struct ForceReference;
	template<typename T> using ForceReference_t = typename ForceReference<T>::type;


	template<typename Base>
	class lambda_state_base_t: public Base {
	public:
		template <typename ... Types, std::size_t ... Indices>
		lambda_state_base_t(const std::tuple<Types...> &tuple, std::index_sequence<Indices...>)
			:Base(std::get<Indices>(tuple)...) {}


	};



	template<typename Base, typename ... Args>
	class lambda_state_t: public lambda_state_base_t<Base> {
	public:
		lambda_state_t(_details::ForceReference_t<Args> ... args)
			:lambda_state_base_t<Base>(std::tuple<Args...>(std::forward<Args>(args)...),std::index_sequence_for<Args...>())
			,params(std::forward<_details::ForceReference_t<Args> >(args)...) {}

		lambda_state_t(const lambda_state_t &other)
			:lambda_state_base_t<Base>(other.params, std::index_sequence_for<Args...>())
			,params(params) {}

		lambda_state_t(lambda_state_t &&other)
			:lambda_state_base_t<Base>(other.params, std::index_sequence_for<Args...>())
			,params(std::move(params)) {}

	protected:
		std::tuple<Args...> params;
	};

}

///Helps to define state variable of the shared function for immovable objects
/**
 * Immovable objects cannot be used in capture list, because the object of the
 * lamba function is moved or copied several times to achieve creation of
 * the shared_function. The lambda_state provides "fake" moving and "fake" copying
 * constructor. When immovable object is copied or moved, it
 * is constructed again using initail arguments.
 *
 * Note that using lambda_date is slow, because object is create and destroyed
 * by several time. If the object is movable, you don't need to use
 * lambda_state.
 *
 * Following example creates mutex as internal state of the function. While
 * the function can be shared, the mutex protects content of the function
 * while it is called by multiple threads.
 *
 *
 * @code
 * auto fn = [lock = lambda_state<std::mutex>()]() mutable {
 *    std::unique_lock<std_mutex> locked(lock);
 *    //... some content
 * };
 * @endcode
 *
 * @tparam T object to create as internal lambda's state
 * @param args optional arguments to construct the object
 * @return
 */
template<typename T, typename ... Args>
auto lambda_state(Args && ... args) {
	return _details::lambda_state_t<T, Args ... >(std::forward<Args>(args)...);
}

///Shared function is function shared to many places, while it maintains its internal state
/**
 * Shared functions have purpose to maintain its internal state. Common function
 * are copied everytime the variable is copied including of their internal state.
 * This is also case of std::function.
 *
 * shared_function is not copied, it is shared, so its internal state is kept
 * between copies.
 */

template<typename Proto>
class shared_function;



template<typename Ret, typename ... Args>
class shared_function<Ret(Args...)> {
private:

	class FnWrapBase: public RefCntObj {
	public:
		virtual Ret call(const shared_function &self, _details::ForceReference_t<Args> ...args) const = 0;
		virtual ~FnWrapBase() {}
	};


	template<typename Fn>
	class FnWrap: public FnWrapBase {
	public:

		typedef std::remove_reference_t<Fn> RFn;

		FnWrap(Fn &&fn):fn(std::forward<Fn>(fn)) {}
		virtual Ret call(const shared_function &self, _details::ForceReference_t<Args> ...args) const {
			return static_cast<Ret>(call2(self,std::forward<_details::ForceReference_t<Args> >(args)...));
		}

		template<typename ... X>
		auto call2(const shared_function &self, X && ...args) const -> decltype(std::declval<RFn>()(std::forward<X>(args)...)) {
			return fn(std::forward<X>(args)...);
		}

		template<typename ... X>
		auto call2(const shared_function &self, X && ...args) const-> decltype(std::declval<RFn>()(self,std::forward<X>(args)...)) {
			return fn(self,std::forward<X>(args)...);
		}

	protected:
		mutable std::remove_reference_t<Fn> fn;
	};

public:

	template<typename T>
	shared_function(T &&fn):fn(new FnWrap<T>(std::forward<T>(fn))) {}
	shared_function(const shared_function &other):fn(other.fn) {}
	shared_function(shared_function &other):fn(other.fn) {}
	shared_function(shared_function &&other):fn(std::move(other.fn)) {}
	shared_function() {}

	shared_function &operator=(const shared_function &other) {
		this->fn = other.fn;
		return *this;
	}

	shared_function &operator=(shared_function &&other) {
		this->fn = std::move(other.fn);
		return *this;
	}


	bool operator==(const shared_function &other) const {return fn == other.fn;}
	bool operator!=(const shared_function &other) const {return fn != other.fn;}
	bool operator>=(const shared_function &other) const {return fn >= other.fn;}
	bool operator<=(const shared_function &other) const {return fn <= other.fn;}
	bool operator>(const shared_function &other) const {return fn > other.fn;}
	bool operator<(const shared_function &other) const {return fn < other.fn;}

	bool operator==(std::nullptr_t) const {return fn == nullptr;}
	bool operator!=(std::nullptr_t) const {return fn != nullptr;}

	bool operator!() const {return fn == nullptr;}

	Ret operator()(_details::ForceReference_t<Args> ... args) const {
		if (fn == nullptr) throw std::bad_function_call();
		return fn->call(*this,std::forward<_details::ForceReference_t<Args> >(args)...);
	}

protected:
	RefCntPtr<FnWrapBase> fn;


};

namespace _details {
	template<typename T> struct ForceReference {
		typedef const T &type;
	};
	template<typename T> struct ForceReference<T &> {
		typedef T &type;
	};
	template<typename T> struct ForceReference<T &&> {
		typedef T &&type;
	};
	template<typename T> struct ForceReference<T *> {
		typedef T *type;
	};

}


}




#endif /* ONDRA_SHARED_H_SHARED_FUNCTION_H_23232idewdkwp3dksmc */
