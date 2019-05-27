/*
 * scheduler.h
 *
 *  Created on: Apr 29, 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_WORKER_SCHEDULER_H_
#define ONDRA_SHARED_WORKER_SCHEDULER_H_
#include <thread>
#include <queue>
#include <functional>

#include "dispatcher.h"
#include "refcnt.h"
#include "waitableEvent.h"


namespace ondra_shared {

template<typename T> class FutureFromType;


template<typename TimePoint>
class AbstractScheduler: public RefCntObj {
public:
	typedef typename TimePoint::duration Duration;


	typedef Dispatcher::Msg Msg;

	///schedule running a function at given timepoint
	/**
	 * @param tp timepoint specifies when the function will run
	 * @param msg function which is executed at given point
	 * @return identifier, which can be used to remove scheduled event
	 */
	virtual std::size_t at(const TimePoint &tp, const Msg &msg)	= 0;

	///Schedule repeating event
	virtual std::size_t each(const Duration &tp, const Msg &msg)	= 0;

	///Removes scheduled event
	/**
	 * @param id identifier of scheduled function
	 * @param callback function called when the operation is complete. The argument can be nullptr.
	 *    The callback function is called in the scheduler's thread.
	 *
	 * @note the operation of removing scheduled function is run asynchronously. If you need
	 * to know, whether the scheduled function has been found and removed, you need to
	 * specify a callback function, which is called with the result. If the function remove()
	 * is called when the specified scheduled function is being executed, the callback function
	 * is called with the argument "false", but after the scheduled function finishes
	 *
	 * @note function is slow. It has linear complexity.
	 */
	virtual void remove(std::size_t id, std::function<void(bool)> callback = nullptr) = 0;

	///Executes message immediate
	/**
	 * @param msg message (function) called immediate
	 *
	 * @note Immediate message is called as soon as code returns the control to the scheduler.
	 * Also note that immediate message cannot be removed
	 */
	virtual void immediate(const Msg &msg) = 0;

	///destructor
	virtual ~AbstractScheduler() {}
};



///Scheduler - provides asynchronous wait operation or scheduling task at specified time
/**
 *
 */
template<typename TimePoint>
class SchedulerT {
public:

	typedef typename TimePoint::clock Clock ;
	typedef typename TimePoint::duration Duration;

	typedef typename AbstractScheduler<TimePoint>::Msg Msg;


	///Constructs empty scheduler's variable
	/** The constructor doesn't initialize the scheduler. You need to call function create()
	 *
	 */
	SchedulerT() {}

	///Initializes scheduler's variable with instance custom scheduler
	explicit SchedulerT(AbstractScheduler<TimePoint> *impl):impl(impl) {}


	///Removes the scheduled item
	/**
	 * @param id identifier of the scheduled item. Both one-time and repeated item are suppotred
	 * @param cb a callback function which is called once removing is complete. It carries
	 * the result of the operation. The value true means, that item has been removed, The value false
	 * means, that item was not found
	 */
	void remove(std::size_t id, const std::function<void(bool)> &cb = nullptr) const {
		impl->remove(id, cb);
	}


	///Basic implementation
	class BasicScheduler: public AbstractScheduler<TimePoint> {

		struct ScheduledItem {
			TimePoint tp;
			Duration interval;
			Msg msg;
			std::size_t id;

			ScheduledItem(const TimePoint &tp,const Duration &interval
					,const Msg &msg, std::size_t id):tp(tp),interval(interval),msg(msg),id(id) {}
		};

		struct LessScheduledItem {
			bool operator()(const ScheduledItem &a, const ScheduledItem &b) {
				return a.tp > b.tp;
			}
		};

		typedef std::priority_queue<ScheduledItem, std::vector<ScheduledItem>, LessScheduledItem> SchQueue;

	public:

		enum _Standalone {standaloneMode};
		enum _Install {installMode};


		BasicScheduler(_Standalone) {
				WaitableEvent ev(false);
				std::thread([&]{
					worker([&]{ev.signal();});
				}).detach();
				ev.wait();
		}

		BasicScheduler(_Install) {
				this->queue = nullptr;
				this->dispatcher = nullptr;
		}


		static bool dispatcher_pump_or_wait_until(Dispatcher &dispatcher, const TimePoint &tp) {
			if (tp == TimePoint::max()) {
				return dispatcher.pump();
			} else {
				return dispatcher.pump_or_wait_until(tp);
			}
		}

		template<typename InitFn>
		void worker(InitFn && initFn) {
			Dispatcher dispatcher;
			SchQueue queue;
			this->queue = &queue;
			this->dispatcher = &dispatcher;
			initFn();

			TimePoint tp = Clock::now();
			TimePoint nx = execAllRetired(queue,tp);
			while (dispatcher_pump_or_wait_until(dispatcher, nx)) {
				TimePoint tp = Clock::now();
				nx = execAllRetired(queue, tp);
			}
		}

		template<typename Fn>
		void install(Fn &&fn) {
			worker([&]{
				fn(SchedulerT(this));
			});
		}

		virtual std::size_t at(const TimePoint &tp, const Msg &msg) override {
			std::size_t id = ++idcounter;
			SchQueue *q = queue;
			dispatcher->dispatch([id, itm = ScheduledItem(tp,Duration::zero(),msg,id),q]{
				q->push(itm);
			});
			return id;
		}

		virtual std::size_t each(const Duration &dur, const Msg &msg) override {
			std::size_t id = ++idcounter;
			TimePoint tp = Clock::now()+dur;
			SchQueue *q = queue;
			dispatcher->dispatch([id, itm = ScheduledItem(tp,dur,msg,id),q]{
				q->push(itm);
			});
			return id;
		}

		virtual void immediate(const Msg &msg) override {
			dispatcher->dispatch(msg);
		}


		virtual void remove(std::size_t id, std::function<void(bool)> cb) override {
			SchQueue *q = queue;
			dispatcher->dispatch([id, cb, q]{

				SchQueue old;
				std::swap(*q,old);
				bool success = false;
				while (!old.empty()) {
					const ScheduledItem &itm = old.top();
					if (itm.id != id) {
						q->push(itm);
						old.pop();
					} else {
						success = true;
						old.pop();
						if (old.size() > q->size()) {
							std::swap(*q,old);
						}
					}

				}
				if (cb != nullptr) cb(success);


			});

		}

		~BasicScheduler() {
			dispatcher->quit();
		}

	protected:

		static TimePoint execAllRetired(SchQueue &q, const TimePoint &curTime) noexcept {
			Duration z(Duration::zero());
			while (!q.empty()) {
				const ScheduledItem &itm = q.top();
				if (itm.tp > curTime) return itm.tp;
				itm.msg();
				if (itm.interval > z) {
					q.push(ScheduledItem(curTime+itm.interval,itm.interval,itm.msg,itm.id));
				}
				q.pop();
			}
			return TimePoint::max();
		}



		SchQueue *queue;
		Dispatcher *dispatcher;
		std::atomic<std::size_t> idcounter;

	};


	///Extends Future object with ability to report the ID of scheduled function
	template<typename Fut>
	class FutureWithID: public Fut {
	public:
		using Fut::Fut;

		///Sets ID
		/**
		 * Function is called by at() or after() to initialize ID
		 * @param id
		 */
		void set_id(std::size_t id) {this->id = id;}
		///Retrieves ID
		/** Retrieves id of scheduled item */
		std::size_t get_id() const {return id;}
	protected:
		std::size_t id;
	};

	///Helper for function at and after
	class At {
	public:
		At(SchedulerT sch, const TimePoint &tp):sch(sch),tp(tp) {}

		template<typename Fn>
		auto operator>>(Fn &&fn) {
			typedef FutureFromType<decltype(std::declval<Fn>()())> FutFromType;
			FutureWithID<typename FutFromType::type> fut;
			fut.set_id(sch.impl->at(tp,[fut,fn=std::remove_reference_t<Fn>(fn)]() mutable {
				fut.set_result_of(fn);
			}));
			return fut;
		}

		SchedulerT sch;
		TimePoint tp;
	};

	///Helper for function each
	class Each {
	public:
		Each(SchedulerT sch, const Duration &dur):sch(sch),dur(dur) {}

		template<typename Fn>
		std::size_t operator>>(Fn &&fn) {
			return sch.impl->each(dur,std::remove_reference_t<Fn>(fn));
		}

		SchedulerT sch;
		Duration dur;

	};

	class Immediate {
	public:
		Immediate(SchedulerT sch):sch(sch) {}

		template<typename Fn>
		void operator>>(Fn &&fn) {
			return sch.impl->immediate(std::remove_reference_t<Fn>(fn));
		}

		SchedulerT sch;
	};

	///Schedules a function at specified time
	/**
	 * To use this function, use the operator >> to assign the function
	 * @code
	 * auto sch = Scheduler.create();
	 * auto future_result = sch.at(tp) >> [=] {
	 *   //function's body;
	 *   return 42;
	 * };
	 * @endcode
	 *
	 * @param tp specifies time point in the future. If the tp is in the pass, the function is
	 * executed immediatelly (but still in scheduler's thread)
	 *
	 * @return Returns extended Future object (FutureWithID). It can be used the same
	 * way as Future, with ability to retrieve ID of the scheduled item. get_id()
	 */
	At at(const TimePoint &tp) const {
		return At(*this, tp);
	}
	///Schedules a function after specified duration
	/**
	 * To use this function, use the operator >> to assign the function
	 * @code
	 * auto sch = Scheduler.create();
	 * auto future_result = sch.after(10s) >> [=] {
	 *   //function's body;
	 *   return 42;
	 * };
	 * @endcode
	 *
	 * @param dur duration
	 *
	 * @return Returns extended Future object (FutureWithID). It can be used the same
	 * way as Future, with ability to retrieve ID of the scheduled item. get_id()
	 */
	template<typename Dur>
	At after(Dur &&dur) const {
		return At(*this, Clock::now()+dur);
	}

	///Schedules repeating function call
	/**
	 *
	 * 	 * To use this function, use the operator >> to assign the function
	 * @code
	 * auto sch = Scheduler.create();
	 * auto id = sch.each(1s) >> [] {
	 * 	 	std::cout << "Called each second" << std::endl;
	 * };
	 * @endcode
	 *
	 *
	 * @param dur interval of each cycle.
	 * @return Returns id of scheduled item.
	 */
	template<typename Dur>
	Each each(Dur &&dur) const {
		return Each(*this, std::chrono::duration_cast<Duration>(dur));
	}


	///Schedules function to run immediate after the current code returns to the scheduler
	/**
	 * 	 * To use this function, use the operator >> to assign the function
	 * @code
	 * auto sch = Scheduler.create();
	 * sch.immediate() >> [] {
	 * 	 	std::cout << "Ran immediate" << std::endl;
	 * };
	 * @endcode
	 *
	 * @note Function scheduled immediately cannot be canceled, so function doesn't return.
	 */
	Immediate immediate() const {
		return Immediate (*this);
	}

	///clears scheduler's variable which decrements count of references
	void clear() {
		impl = nullptr;
	}

	///Creates scheduler
	/**
	 * @return Returns scheduler
	 *
	 * @note to destroy scheduler, decrement count of references to zero (you can use
	 * clear() on initialized variable). Once this is achieved, scheduler is destroyed.
	 */
	static SchedulerT create() {
		return SchedulerT(new BasicScheduler(BasicScheduler::standaloneMode));
	}

	///Installs the scheduler to the current thread
	/**
	 * Function converts current thread to scheduler's thread. This allows to have scheduler
	 * without need to create any thread.
	 *
	 * @param init Function is called to initialize scheduler's thread. The function receives
	 * Scheduler variable containing reference to the instance. You have chance
	 * to schedule the first function. The scheduler starts once the function is left.
	 *
	 * To uninstall the scheduler, simply decrement count of references to zero. You can use
	 * Scheduler::clear() to achieve this. Once this is achieved, the scheduler uninstalls
	 * itself as soon as the control is returned to the scheduler's level
	 */
	template<typename InitFn>
	static void install(InitFn && init) {
		(new BasicScheduler(BasicScheduler::installMode))->install(init);
	}

	///Returns true, if object is valid (i.e. has assigned scheduler object)
	bool valid() const {return impl != nullptr;}

	///Synchronizes execution with the scheduler
	/** useful to ensure,that operation has been removed and it is not executed right now */
	void sync() {
		Countdown ctn(1);
		immediate() >> [&]{ctn.dec();};
		ctn.wait();
	}

protected:
	RefCntPtr<AbstractScheduler<TimePoint> > impl;

};

///Scheduler, for documentation, see SchedulerT
using Scheduler = SchedulerT<std::chrono::time_point<std::chrono::steady_clock> >;


}


#endif /* SCHEDULER_H_ */
