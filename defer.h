/*
 * defer.h
 *
 *  Created on: 25. 5. 2018
 *      Author: ondra
 */

#ifndef SRC_ONDRA_SHARED_DEFER_H_017498198781
#define SRC_ONDRA_SHARED_DEFER_H_017498198781

#include <utility>


namespace ondra_shared {

template<typename T> class Future;
template<typename T> class FutureFromType;
template<typename T> class FutureExceptionalState;



class IDeferFunction {
public:
	virtual void run() noexcept= 0;
	virtual ~IDeferFunction() noexcept {}

	IDeferFunction *next;
};

class IDeferContext {
public:


	static thread_local IDeferContext *include_defer_tcc_to_your_main_source;

	template<typename Fn>
	void defer_call(Fn && fn) {
		class FnObj: public IDeferFunction {
		public:
			FnObj(Fn && fn):fn(std::forward<Fn>(fn)) {}
			virtual void run() noexcept override {
				fn();
			}
			~FnObj() noexcept override {}

		protected:
			std::remove_reference_t<Fn> fn;
		};

		IDeferFunction *dfn = new FnObj(std::forward<Fn>(fn));
		defer_call_impl(dfn);
	}
	template<typename Fn>
	void operator>>(Fn &&fn) {
		defer_call<Fn>(std::forward<Fn>(fn));
	}

	template<typename Fn,
			typename RetVal = typename FutureFromType<decltype(std::declval<Fn>()())>::type >
	RetVal operator>>=(Fn &fn) {
		RetVal tmp;
		(*this) >> [fn = std::remove_reference_t<Fn>(fn), tmp]() mutable {

		};

	}
protected:
	virtual void defer_call_impl(IDeferFunction *fn) = 0;


};


enum DeferRootKW {
	defer_root
};

class DeferStack: public IDeferContext {
public:

	DeferStack()
		:prevContext(this),top(nullptr) {

	}

	explicit DeferStack(DeferRootKW)
		:prevContext(include_defer_tcc_to_your_main_source)
		,top(nullptr) {
		include_defer_tcc_to_your_main_source = this;
	}

	~DeferStack() noexcept {
		flush();
		if (prevContext != this)
			include_defer_tcc_to_your_main_source = prevContext;
	}

	void flush() {
		while (top) {
			IDeferFunction *x = top;
			top= x->next;
			x->run();
			delete x;
		}
	}

	virtual void defer_call_impl(IDeferFunction *fn) {
		fn->next = top;
		top = fn;
	}



protected:
	IDeferContext *prevContext;
	IDeferFunction *top;
};




class DeferContext: public DeferStack {
public:

	DeferContext():last(nullptr) {}
	DeferContext(DeferRootKW):DeferStack(defer_root), last(nullptr) {}

	~DeferContext() noexcept {
		flush();
	}

	virtual void defer_call_impl(IDeferFunction *fn) {
		if (top) {
			last->next = fn;
			last = fn;
		} else {
			top= last = fn;
		}
		fn->next = nullptr;
	}

protected:
	IDeferFunction *last;

};

enum DeferKeyword {
	defer
};

template<typename Fn>
void operator>>(DeferKeyword, Fn &&fn) {
	IDeferContext *curContext = IDeferContext::include_defer_tcc_to_your_main_source;
	if (curContext == nullptr) {
		DeferContext newContext(defer_root);
		newContext.defer_call(std::forward<Fn>(fn));
	} else {
		curContext->defer_call(std::forward<Fn>(fn));
	}
}





};





#endif /* SRC_ONDRA_SHARED_DEFER_H_017498198781 */
