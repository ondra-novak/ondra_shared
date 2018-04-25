/*
 * worker.h
 *
 *  Created on: 25. 4. 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_WORKER_H_456106749845
#define ONDRA_SHARED_WORKER_H_456106749845

#include "dispatcher.h"
#include "mtcounter.h"
#include "refcnt.h"
#include "apply.h"

namespace ondra_shared {



class AbstractWorker: public RefCntObj {
public:

	typedef Dispatcher::Msg Msg;

	virtual void dispatch(const Msg &msg)	= 0;

	virtual ~AbstractWorker() {}
};

class AbstractThreadedWorker: public AbstractWorker {
public:

	virtual void addThread() = 0;
	virtual void addThisThread() = 0;
	virtual void stopAllThreads() = 0;

};

template<typename T> class Future;
template<typename T> class FutureFromType;
template<typename T> class FutureExceptionalState;


///Creates event driven worker running in separate thread (or threads)
/**
 * Worker is a single thread or pool of multiple threads and single dispatcher. You can
 * dispatch various events to the worker's thread(s). These events are represented as functions
 * that are executed in context of the worker's thread.
 *
 * Worker object is allocated on heap and acts as shared resource. Once the worker is created, you
 * can transfer it through variables, pass it as value to the functions or to the lambda functions.
 * It cannot be copied, so it is always one instance shared at many places. The worker can
 * cleanup itself after the last reference is released. Also note, that enqueued functions
 * are also counted as references. This means you can enqueue a function and then discard the
 * refernce without worrying about destroying worker without processing the the function. The
 * worker deletes self once there is none of references and there is no work in its queue as well.
 *
 * To create worker, call Worker::create(), Then you can call dispatch() or use operator>>
 * to enqueue a work
 *
 * @code
 * auto worker = Worker::create();
 * worker.dispatch([=]{
 *    //function to execute in worker
 * })
 * @endcode
 *
 * The operator >> allows to combine worker with futures
 *
 * @code
 * auto worker = Worker::create()
 * worker >> [=]{
 	 	 int result = .. some calculation
 * 		return result;
 * } >> [=] (int res) {  //then
 *     //process result
 *
 * }
 * @endcode
 *
 * You can also use a worker to transfer processing chained code into worker's thread
 *
 * @code
 * auto worker = Worker::create();
 * doSomethingAsync() >> w >> [=](Result res) {
 *   //pick up the result res
 * };
 * @endcode
 *
 * The sequence >> w >> instructs the code to pass result to the worker and call the function
 * in the worker's thread, so the pick-up-result function will not block the thread
 * of the async operation.
 *
 *
 * Worker is created with single thread only. If you need to create multiple thread worker, you
 * need to specify count of threads as argument of the function create();
 *
 * @code
 * auto worker_8threads = Worker::create(8)
 * @endcode
 *
 *
 * Inside of worker's thread, you can always access the current worker's instance by calling
 * Worker::getCurrent().
 *
 */

class Worker {
public:

	typedef Dispatcher::Msg Msg;

	///Create empty worker's variable
	Worker() {}
	///Initialize a worker's variable with custrom instance
	explicit Worker(AbstractWorker *wrk):wrk(wrk) {}

	///Default worker implementation
	class DefaultWorker: public AbstractThreadedWorker {
	public:

		virtual void dispatch(const Msg &msg)	{
			if (tc.getCounter() == 0) {
				if (tc.setCounterWhen(0,1)) {
					newThread();				}
			}
			addRef();
			d.dispatch(msg);
		}

		virtual void addThread() {
			tc.inc();
			newThread();
		}
		virtual void addThisThread() {
			tc.inc();
			Worker::setCurrent(this);
			worker();
			Worker::clearCurrent();

		}
		virtual void stopAllThreads() {
			d.quit();
			tc.wait();
			d.clear();
		}


		~DefaultWorker() {
			stopAllThreads();
		}

		void worker() noexcept {
			while (d.pump()) {
				if (release()) {
					tc.dec();
					delete this;
					return;
				}
			}
			d.quit();
			tc.dec();
		}


	protected:
		Dispatcher d;
		MTCounter tc;

		void newThread() {
			std::thread thr([=]{
				Worker::setCurrent(this);
				worker();
			});
			thr.detach();
		}
	};

	///Creates default single threaded worker
	/** Function doesn't start thread unless first message is enqueued */
	static Worker create() {
		return Worker(new DefaultWorker);

	}

	///Creates multithreaded worker
	/**
	 * @param threads count of desired threads
	 * @return worker with threads
	 *
	 * @note function immediatelly initializes all desired threads.
	 */
	static Worker create(unsigned int threads) {
		RefCntPtr<AbstractThreadedWorker> w = new DefaultWorker;
		for (unsigned int i = 0; i < threads; i++) w->addThread();
		return Worker(RefCntPtr<AbstractWorker>::staticCast(w));
	}

	///Adds new thread to the worker
	/**
	 * This creates new thread and registers it to the pool
	 * @note you cannot remove thread. You can only stop all threads
	 */
	void addThread() {
		dynamic_cast<AbstractThreadedWorker &>(*wrk).addThread();
	}
	///Adds this thread as the worker
	/**
	 * Instead of the creating new thread, this thread becomes a worker's thread. The
	 * funcction exits after the worker stops using the thread.
	 */
	void addThisThread() {
		dynamic_cast<AbstractThreadedWorker &>(*wrk).addThisThread();
	}
	///Stops all threads
	/**
	 * Function requests all threads to stop and blocks to complette the request. After stopping
	 * the threds, the worker has no running thread, so you can add new set of threads. If
	 * you enqueue a message to such worker, a single thread is automatically created.
	 *
	 * @note Funtion is not MT safe. Do not use other functions while this function is in progress
	 *
	 */
	void stopAllThreads() {
		dynamic_cast<AbstractThreadedWorker &>(*wrk).stopAllThreads();
	}

	///Returns true, if the variable contains a worker instance
	bool defined() const {
		return wrk != nullptr;
	}

	///Returns current worker
	/** The current worker is defined inside of the worker's thread. Function throws
	 * exception outside unless you set the current worker for the current thread. See
	 * setCurrent()
	 *
	 * @return current worker variable
	 * @exception runtime_error "There is no current worker available"
	 */
	static Worker getCurrent() {
		AbstractWorker *wrk = _currentWorker();
		if (wrk == nullptr)
			throw std::runtime_error("There is no current worker available");
		return Worker(wrk);
	}

	///Makes specified worker as current for the current thread
	/**
	 * @param wrk worker variable. Use empty worker's variable to unset current worker
	 *
	 * @note By setting the worker as current is not counted as reference. You still need
	 * to keep at least one reference. If the worker is destroyed without unsetting the
	 * current worker, the result of getCurrent() is undefined.
	 *
	 */
	static void setCurrent(const Worker &wrk) {
		_currentWorker() = wrk.wrk;
	}

	///Makes current worker's implementation as active
	static void setCurrent(AbstractWorker *wrk) {
		_currentWorker() = wrk;
	}

	///Removes current worker
	static void clearCurrent() {
		_currentWorker() = nullptr;
	}

	///dispatch a single function
	/** calls the function in context of the worker */
	void dispatch(const Msg &msg) {
		wrk->dispatch(msg);
	}

	///Call the function asynchronously in the context of the worker and return Future
	/**
	 * @param fn function to call
	 * @return Future containing return value of the function
	 */
	template<typename Fn>
	auto operator>>(Fn &&fn) {
		typename FutureFromType<decltype(std::declval<Fn>()())>::type fut;
		dispatch([fut,f = std::remove_reference_t<Fn>(fn)] ()mutable {
			try {
				fut.set(f());
			} catch (...) {
				fut.reject();
			}
		});
		return fut;
	}


protected:

	RefCntPtr<AbstractWorker> wrk;

	static AbstractWorker *& _currentWorker() {
		static thread_local AbstractWorker *w;
		return w;
	}
};


namespace _details {
template<typename Fut>
class WorkerFutureChain {
public:
	template<typename FutR, typename WrkR>
	WorkerFutureChain(FutR && fut, WrkR && wrk):fut(fut),wrk(wrk) {}

	template<typename Fn>
	auto operator >> (Fn &&fn) {
		typedef std::remove_reference_t<decltype(fut.get())> ValueType;

		return fut >> [wrk = this->wrk, fn = std::remove_reference_t<Fn>(fn)](const ValueType & vt) mutable {
			Future<ValueType> fut;
			auto res = fut >> fn;

			wrk.dispatch([fut,v = ValueType(vt)] () mutable {
				fut.set(v);
			});
			return res;
		};
	}

protected:
	Fut fut;
	Worker wrk;
};

template<typename Fut>
class WorkerFutureExceptionChain {
public:
	template<typename FutR, typename WrkR>
	WorkerFutureExceptionChain(FutR && fut, WrkR && wrk):fut(fut),wrk(wrk) {}

	template<typename Fn>
	auto operator >> (Fn &&fn) {

		return fut.then_catch([wrk = this->wrk,  fn = std::remove_reference_t<Fn>(fn)](const std::exception_ptr & e) mutable {
			typename FutureFromType<decltype(std::declval<Fn>()(e))>::type fut;
			wrk.dispatch([fut,fn, e = std::exception_ptr(e)] () mutable {
					try {
						fut.set(fn(e));
					} catch (...) {
						fut.reject();
					}
			});
			return fut;
		});
	}

protected:
	Fut fut;
	Worker wrk;
};
}
template<typename T>
auto operator >> (const Future<T> &fut, const Worker &wrk) {
	return _details::WorkerFutureChain<Future<T> >(fut, wrk);
}

template<typename T>
auto operator >> (const FutureExceptionalState<Future<T> > &fut, const Worker &wrk) {
	return _details::WorkerFutureExceptionChain<Future<T> >(fut.getFuture(), wrk);
}


}





#endif /* ONDRA_SHARED_WORKER_H_456106749845 */
