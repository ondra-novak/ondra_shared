/*
 * waitableEvent.h
 *
 *  Created on: 24. 4. 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_WAITABLEEVENT_H_20412316476974
#define ONDRA_SHARED_WAITABLEEVENT_H_20412316476974

#include "mtcounter.h"

namespace ondra_shared {

///WaitableEvent is simplified MTCounter, allows to initialize signaled or unsignaled event and wait
/**
 * WaitableEvent works as conditional variable, but it also remembers state and doesn't
 * need additional mutex to work. When event is recorded, the object is set to signaled state.
 * and additional wait() function unblocks the threads
 */
class WaitableEvent {
public:
	WaitableEvent(bool signaled):c(signaled?0:1) {}

	void signal() {
		c.dec();
	}

	void reset() {
		c.setCounter(1);
	}

	void operator()() const {
		c.dec();
	}

	bool wait(unsigned int timeout_ms) {return c.wait(timeout_ms);}
	void wait() {c.wait();}
	template<typename TimePoint> bool wait_until(const TimePoint &tp) {return c.wait_until(tp);}
	template<typename Duration> bool wait_for(const Duration &dur) {return c.wait_for(dur);}

protected:
	mutable MTCounter c;
};



}




#endif /* ONDRA_SHARED_WAITABLEEVENT_H_20412316476974 */
