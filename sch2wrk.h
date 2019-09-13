/*
 * sch2wrk.h
 *
 *  Created on: May 1, 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_SRC_SHARED_SCH2WRK_H_489109337
#define ONDRA_SHARED_SRC_SHARED_SCH2WRK_H_489109337
#include "scheduler.h"
#include "worker.h"

namespace ondra_shared {

template<typename TimePoint>
class WorkerByScheduler: public AbstractWorker {
public:
	WorkerByScheduler(const SchedulerT<TimePoint> &sch):sch(sch) {}

	virtual void dispatch(Msg &&msg){
		sch.immediate() >> std::move(msg);
	}
	virtual void run() noexcept	{}
	virtual void flush() noexcept {
		sch.sync();
	}


protected:
	SchedulerT<TimePoint> sch;
};


///Gets worker's instance from the scheduler
/** Scheduler allows to scheduler function immediatelly,  so it is able to simulate a worker
 *
 * @param sch instance of scheduler
 * @return instance of warker
 */
template<typename TimePoint>
static Worker schedulerGetWorker(const SchedulerT<TimePoint> &sch) {
	return Worker(new WorkerByScheduler<TimePoint>(sch));
}



}




#endif /* ONDRA_SHARED_SRC_SHARED_SCH2WRK_H_489109337 */
