/*
 * shared_function.h
 *
 *  Created on: 12. 10. 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_H_SHARED_FUNCTION_H_23232idewdkwp3dksmc
#define ONDRA_SHARED_H_SHARED_FUNCTION_H_23232idewdkwp3dksmc

#include "refcnt.h"
#include <memory>
#include <type_traits>
#include <functional>

namespace ondra_shared {


namespace _details {
	template<typename T> struct ForceReference;
	template<typename T> using ForceReference_t = typename ForceReference<T>::type;
}

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
	shared_function() {}

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
