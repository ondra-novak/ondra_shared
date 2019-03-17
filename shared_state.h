/*
 * shared_state.h
 *
 *  Created on: 27. 6. 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_SHARED_STATE_H_5546415
#define ONDRA_SHARED_SHARED_STATE_H_5546415

#include "refcnt.h"

namespace ondra_shared {

namespace SharedState {


///Object which is used as shared state
class Object: public RefCntObj {
public:



};

///Declares function which executes shared state as function
/**
 * Shared state is shared between threads. Because it is mostly represented as pointer (shared pointer), it cannot be
 * dispatched through the worker or dispatcher (or Future). This object represents function which can be dispatched. It
 * can be also used as shared pointer. By calling the function the same  function (with the same prototype) on the shared state is executed
 *
 *
 * @code
 * // declare shared state
 *
 * class MyState: public SharedState::Object {
 * public:
 * 	 MyState(Worker worker, int val):worker(worker), sharedVal(val) {}
 *
 *   // contains function which works with shared state
 *   void operator()() {
 *      // example operation
 *      //
 *      // till sharedVal is above zero
 *   	if (--sharedVal > 0)
 *   	   // put self to the worker
 *   	   worker >> SharedState::reuse(this);
 *   }
 *
 *   Worker worker;
 *   atomic<int> sharedVal;
 * };
 *
 * //create 4 threads worker
 * Worker wrk = Worker::create(4);
 * //create shared state object
 * auto mystate = SharedState::make<MyState>(wrk, 1000);
 * //run task paralel on 4 threads
 * for (int i = 0; i < 4; i++) {
 * 		wrk >> mystate;
 * }
 *
 * @endcode
 *
 */
template<typename Object>
class Fn: public RefCntPtr<Object> {
public:

	using RefCntPtr<Object>::RefCntPtr;


	template<typename ... Args >
	auto operator()(Args && ...args) const -> decltype((*std::declval<Object *>())(std::forward<Args>(args)...)) {
		return this->ptr->operator()(std::forward<Args>(args)...);
	}
};

///Constructs new shared state object
/**
 *
 * @param args arguments of shared state
 * @return shared state object, which can be passed to the worker
 */
template<typename T, typename ... Args> static Fn<T> make(Args && ... args) {
	return Fn<T>(new T(std::forward<Args>(args)...));
}
///Reuses shared state
/**
 * This allows to reuse _this_ shared state in new work
 * @param obj pointer to shared state (probably this)
 * @return function which executes task on shared state
 */
template<typename T> static Fn<T> reuse(T *obj) {
	return Fn<T>(obj);
}

}

}



#endif /* SRC_SHARED_SHARED_STATE_H_ */
