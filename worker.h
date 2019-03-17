/*
 * worker.h
 *
 *  Created on: 25. 4. 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_WORKER_H_456106749845
#define ONDRA_SHARED_WORKER_H_456106749845
#include <thread>

#include "dispatcher.h"
#include "mtcounter.h"
#include "refcnt.h"
#include "apply.h"


namespace ondra_shared {


///abstract worker implementation
class AbstractWorker: public RefCntObj {
public:

	typedef Dispatcher::Msg Msg;

	///dispatch the message
	virtual void dispatch(const Msg &msg)	= 0;

	///run the worker for current thread
	virtual void run() noexcept	= 0;

	///run the worker for current thread but doesn't wait for new messages
	virtual void flush() noexcept	= 0;


	///destructor
	virtual ~AbstractWorker() {}
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
 */

class Worker {
public:

	typedef Dispatcher::Msg Msg;

	///Create empty worker's variable
	Worker() {}
	///Initialize a worker's variable with custrom instance
	explicit Worker(AbstractWorker *wrk):wrk(wrk) {}

	class SharedDispatcher: public Dispatcher, public RefCntObj {
	public:
		~SharedDispatcher() {
			run();
		}
	};


	///Default worker implementation
	class DefaultWorker: public AbstractWorker {
	public:

		virtual void dispatch(const Msg &msg)	{
			d->dispatch(msg);
		}

		void addThread() {
			newThread();
		}

		DefaultWorker():d(new SharedDispatcher) {}

		~DefaultWorker() {
			d->quit();
		}

		static void worker(RefCntPtr<SharedDispatcher> sd) {
			sd->run();
			sd->quit();
		}

		virtual void run() noexcept override {
			worker(d);
		}

		virtual void flush() noexcept override {
			RefCntPtr<SharedDispatcher> sd(d);
			while (!sd->empty()) {
				if (!sd->pump()) {
					sd->quit();
					break;
				}
			}
		}

	protected:
		RefCntPtr<SharedDispatcher> d;

		void newThread() {

			std::thread thr(std::bind(&worker,d));
			thr.detach();
		}
	};


	///Creates multithreaded worker
	/**
	 * @param threads count of desired threads. The parameter is 1, so the worker
	 * is initialized with a single thread. If you set this argument to 0, then worker without
	 * own thread is created. Such worker doesn't process its queue until the flush() is called.
	 *
	 * @return worker instance
	 */
	static Worker create(unsigned int threads = 1) {
		RefCntPtr<DefaultWorker> w = new DefaultWorker;
		for (unsigned int i = 0; i < threads; i++) w->addThread();
		return Worker(RefCntPtr<AbstractWorker>::staticCast(w));
	}


	///Installs worker to the current thread
	/**
	 * By installing worker to current thread, the current thread is converted to worker.
	 * This allows to have at least one worker without need to have threading library.
	 *
	 * @param fn Function called to initialize the worker. It receives Worker variable
	 *
	 * @note To uninstall worker, you need to decrement count of references of the
	 * Worker's instance to zero. Once this is achieved, the program only needs to return
	 * from the current function back to the worker.
	 */
	template<typename InitFn>
	static void install(InitFn && fn) {
		DefaultWorker *w = new DefaultWorker;
		w->dispatch([=]{
			fn(Worker(w));
		});
		w->run();
	}


	///Returns true, if the variable contains a worker instance
	bool defined() const {
		return wrk != nullptr;
	}

	///dispatch a single function
	/** calls the function in context of the worker */
	void dispatch(const Msg &msg) const{
		wrk->dispatch(msg);
	}

	///Clears variable
	/** If the worker as a single instance, it causes that worker is destroyed*/
	void clear() {wrk = nullptr;}

	///Flushes worker's queue
	/** Function should be called on workers without own thread. It process all messages
	 * in the queue and returns when there is no unprocessed message. If the worker
	 * as thread(s), this function temporary adds current thread to the pool of threads and
	 * helps the queue.
	 *
	 * @note you should always call flush before the worker without own thread
	 * is destroyed or detached, especially when worker is shared into many places. Without flushing,
	 * the enqueued functions are not called resulting to many resource leaks
	 *
	 */
	void flush() const {wrk->flush();}

	///Converts current thread to worker's thread complete
	void run() const {wrk->run();}


	template<typename Fn>
	void operator >> (Fn &&fn) const {
		dispatch(std::forward<Fn>(fn));
	}

protected:

	RefCntPtr<AbstractWorker> wrk;

};


}





#endif /* ONDRA_SHARED_WORKER_H_456106749845 */
