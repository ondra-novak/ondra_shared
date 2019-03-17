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



///Abstract defer context
class IDeferContext {
public:

	///Abstract interface for defered function. Do not use directly
	class IDeferFunction {
	public:
		virtual void run() noexcept= 0;
		virtual ~IDeferFunction() noexcept {}

		IDeferFunction *next;
	};

	class EmptyDeleter {
	public:
		void operator()(IDeferContext *) const {}
	};

	template<typename T>
	class PointerWrapper {
	public:
		PointerWrapper(T *x):x(x) {}
		T *get() const {return x;}
		bool operator==(const PointerWrapper &other) const {return x == other.x;}
		bool operator!=(const PointerWrapper &other) const {return x != other.x;}
	protected:
		T *x = nullptr;
	};

	//HACK: It seems that GCC has bug to generate properly thread_local initialization in the function getInstance
	//when the pointer to IDeferContext is simple or POD object. We use wrapper class

	using PDeferContext = PointerWrapper<IDeferContext>;


	static PDeferContext &getInstance() {
		static thread_local PDeferContext instance(nullptr);
		return instance;
	}


	///defer function call
	/**
	 * Defered function is called implicitly on defer_root or explicitly on yield()
	 *
	 * @param fn function to call defered
	 *
	 * @note function is not thread safe
	 */
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

	///defer function call
	/**
	 * Defered function is called implicitly on defer_root or explicitly on yield()
	 *
	 * @param fn function to call defered
	 *
	 * @note function is not thread safe
	 */
	template<typename Fn>
	void operator>>(Fn &&fn) {
		defer_call<Fn>(std::forward<Fn>(fn));
	}


	///Execute all defered functions now
	/** You need to call this function if there is posibility that thread will
	 * be blocked or locked waiting for finish operation which has been defered
	 */
	virtual void yield() noexcept= 0;

protected:
	virtual void defer_call_impl(IDeferFunction *fn) = 0;


};


enum DeferRootKW {
	///create new root
	defer_root,
	///create new root if there is none, otherwise keep current root
	defer_root_if_none
};

///Creates defer stack context
/**
 * This creates context, where the defered functions are executed in reverse order of
 * registration.
 *
 * This can be handful for cleanup stack non-RAII items.
 *
 * @code
 * DeferStack defer;
 *
 *  //open file
 * FILE *f = fopen("test"."w");
 *  //defer closing of the file
 * defer >> [f]{fclose(f);};
 *
 *  //.. use file without need to close it explicitly
 *
 * @endcode
 */
class DeferStack: public IDeferContext {
public:

	///Creates local defer stack
	/** Created defer stack can be controlled only through it instance, it has
	 * none impact to global defer feature
	 */
	DeferStack()
		:prevContext(this),top(nullptr) {

	}

	///Creates global defer stack
	/**
	 * Created defer stack affects defer feature for the current thread
	 * @param kw specifies how is defer_root handled
	 */
	explicit DeferStack(DeferRootKW kw)
		:prevContext(IDeferContext::getInstance().get())
		,top(nullptr) {
		if (IDeferContext::getInstance() == nullptr || kw ==  defer_root)
			IDeferContext::getInstance() = PDeferContext(this);
	}

	///Destructor
	/** Destructor also performs implicit yield() */
	~DeferStack() noexcept {
		yield();
		if (prevContext != this)
			IDeferContext::getInstance() = PDeferContext(prevContext);
	}

	virtual void yield() noexcept override {
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



///Creates standard defer context
/**
 * Standard defer context is implemented as FIFO. The first defered function
 * is also first called.
 *
 */
class DeferContext: public DeferStack {
public:

	///Creates local defer context
	/**
	 * Local defer context can be controlled only through its variable. It has no
	 * impact to global context
	 */
	DeferContext():last(nullptr) {}

	///Creates global defer context
	/**
	 * @param kw specifies how the global context is defined
	 */
	DeferContext(DeferRootKW kw):DeferStack(kw), last(nullptr) {}

	~DeferContext() noexcept {
		yield();
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

///The keyword is used to defer a function on global defer context
enum DeferKeyword {
	defer
};

///Defers the function in global defer context
/**
 *
 * @param DeferKeyword always "defer"
 * @param fn function which is called "later".
 *
 * @note if there is no active defer context, the function creates one. However in
 * such situation, the function cannot be defered, so it is called immediatelly. However
 * if the function uses defer anywhere in its body, all these functions are defered and
 * executed after the first function returns
 *
 * The very first use of "defer" is often performed to define implicit defer_root and
 * the all subsequent functions are correctly defered
 */
template<typename Fn>
void operator>>(DeferKeyword, Fn &&fn) {
	IDeferContext *curContext = IDeferContext::getInstance().get();
	if (curContext == nullptr) {
		DeferContext newContext(defer_root);
		fn();
	} else {
		curContext->defer_call(std::forward<Fn>(fn));
	}
}

///Yields current thread and explicitly executes all defered functions
/**
 * This is required to empty defer context when for example the function is going
 * to block or lock the thread while it waiting to an event which can be
 * the result of some defered function. Without it a deadlock can be result of such
 * situation.
 *
 */
inline void defer_yield() noexcept {
	IDeferContext *curContext = IDeferContext::getInstance().get();
	if (curContext) curContext->yield();
}



};





#endif /* SRC_ONDRA_SHARED_DEFER_H_017498198781 */
