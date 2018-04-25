/*
 * dispatchqueue.h
 *
 *  Created on: Sep 9, 2017
 *      Author: ondra
 */

#ifndef __ONDRA_SHARED_DISPATCHER_54789465163518_
#define __ONDRA_SHARED_DISPATCHER_54789465163518_


#include <functional>
#include "msgqueue.h"

namespace ondra_shared {

///Queue which contains function to dispatch (message loop)
class Dispatcher {
public:

	typedef std::function<void()> Msg;
protected:
	MsgQueue<std::function<void()> > queue;

public:
	///starts message loop. Function processes messages
	void run() {
		while(pump()) {}
	}

	///Checks whether message queu is empty
	/**
	 * @retval true is empty
	 * @retval false there is a message
	 *
	 * You can use this function to check queue before you call pump(). If the result is false,
	 * the function pump() will not block waiting for the message
	 *
	 */
	bool empty() const {
		return queue.empty();
	}

	///pumps one message. It blocks waiting for the message when the queue is empty
	/**
	 *
	 * @retval true a single message processed
	 * @retval false the quit message extracted
	 */
	bool pump() {
		Msg a = queue.pop();
		if (a != nullptr) {
			a();
			return true;
		} else {
			return false;
		}
	}


	///quits the dispatcher
	/**
	 * Post special message to the queue requesting the dispatcher to stop processing the messages
	 *
	 * Special message is not processed during the pump, it only causes, that pump() returns false.
	 *
	 */
	void quit() {
		queue.push(nullptr);
	}


	///Dispatch the function
	void dispatch(const Msg &msg) {
		queue.push(msg);
	}

	///dispatch function
	void operator<<(const Msg &msg) {
		queue.push(msg);
	}

	///clear the queue
	/** Dispatching thread should clear the queue on exit. Otherwise the
	 * dispatched messages are cleared by the destructor
	 */
	void clear() {
		queue.clear();
	}

	///allows to use syntax function >> dispatcher
	friend void operator>>(const Msg &msg, Dispatcher &dispatcher) {
		dispatcher << msg;
	}

};


}
#endif
