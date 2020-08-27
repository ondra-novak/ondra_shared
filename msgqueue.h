#ifndef __ONDRA_SHARED_MSGQUEUE_H_50860843615561_
#define __ONDRA_SHARED_MSGQUEUE_H_50860843615561_


#include <condition_variable>
#include <mutex>
#include <queue>


namespace ondra_shared {


template<typename Msg, typename QueueImpl = std::queue<Msg> >
class MsgQueue {
public:

	///Push message to the queue (no blocking)
	void push(const Msg &msg);

	///Push message to the queue (no blocking)
	void push(Msg &&msg);

	///Pop message from the queue
	/** Function blocks if there is no message
	 *
	 * @return message retrieved from the queue
	 */
	Msg pop();

	///Determines whether queue is empty
	bool empty() const;

	///checks the queue, If there is an message, it calls the function with the message as an argument
	/**
	 *
	 * @param fn function called with the message
	 * @retval true pumped a message
	 * @retval false there were no message
	 */
	template<typename Fn>
	bool try_pump(Fn fn);

	///Pumps one message or block the thread
	/**
	 * Function works similar as pop(), but the message is not returned, instead it
	 * is passed to the function as an argument
	 *
	 * @param fn function called with message
	 *
	 * @note only one message is processed
	 */
	template<typename Fn>
	void pump(Fn fn);

	///Pumps one message or blocks the thread for the given period of the time
	/**
	 *
	 * @param rel_time relative time - duration
	 * @param fn function called with the message
	 * @retval true pumped a message
	 * @retval false there were no message and timeout ellapsed
	 */
	template< typename Duration, class Fn>
	bool pump_for(Duration && rel_time, Fn fn);

	///Pumps one message or blocks the thread for the given period of the time
	/**
	 *
	 * @param rel_time relative time - duration
	 * @param fn function called with the message
	 * @retval true pumped a message
	 * @retval false there were no message and timeout ellapsed
	 */
	template< typename TimePoint, class Fn >
	bool pump_until(TimePoint && timeout_time,Fn pred );


	///Allows to modify content of the queue.
	/** Function locks the object and calls the function with the instance
	 * of the queue as the argument. Function can modify content of the queue.
	 * The queue is unlocked upon the return
	 * @param fn function called to modify the queue.
	 */
	template<typename Fn>
	void modifyQueue(Fn fn);

	void clear();


protected:
	QueueImpl queue;
	mutable std::recursive_mutex lock;
	std::condition_variable_any condvar;
	typedef std::unique_lock<std::recursive_mutex> Sync;
};


template<typename Msg, typename QueueImpl>
inline void MsgQueue<Msg, QueueImpl>::push(const Msg& msg) {
	Sync _(lock);
	queue.push(msg);
	condvar.notify_one();
}

template<typename Msg, typename QueueImpl>
inline void MsgQueue<Msg, QueueImpl>::push(Msg&& msg) {
	Sync _(lock);
	queue.push(std::move(msg));
	condvar.notify_one();
}

template<typename Msg, typename QueueImpl>
inline Msg MsgQueue<Msg, QueueImpl>::pop() {
	Sync _(lock);
	condvar.wait(_, [&]{return !queue.empty();});
	Msg ret (std::move(queue.front()));
	queue.pop();
	return ret;
}

template<typename Msg, typename QueueImpl>
inline bool MsgQueue<Msg, QueueImpl>::empty() const{
	Sync _(lock);
	return queue.empty();
}

template<typename Msg, typename QueueImpl>
template<typename Fn>
inline bool MsgQueue<Msg, QueueImpl>::try_pump(Fn fn) {
	Sync _(lock);
	if (queue.empty()) return false;
	Msg ret (std::move(queue.front()));
	queue.pop();
	_.unlock();
	fn(std::move(ret));
	return true;
}

template<typename Msg, typename QueueImpl>
template<typename Fn>
inline void MsgQueue<Msg, QueueImpl>::pump(Fn fn) {
	Sync _(lock);
	condvar.wait(_, [&]{return !queue.empty();});
	Msg ret (std::move(queue.front()));
	queue.pop();
	_.unlock();
	fn(std::move(ret));
}

template<typename Msg, typename QueueImpl>
template<typename Duration, class Fn>
inline bool MsgQueue<Msg, QueueImpl>::pump_for(
		Duration&& rel_time, Fn fn) {
	Sync _(lock);
	if (!condvar.wait_for(_, std::forward<Duration>(rel_time), [&]{return !queue.empty();})) return false;
	Msg ret (std::move( queue.front()));
	queue.pop();
	_.unlock();
	fn(std::move(ret));
	return true;
}

template<typename Msg, typename QueueImpl>
template<typename TimePoint,  class Fn>
inline bool MsgQueue<Msg, QueueImpl>::pump_until(
		TimePoint &&timeout_time, Fn fn) {
	Sync _(lock);
	if (!condvar.wait_until(_, std::forward<TimePoint>(timeout_time), [&]{return !queue.empty();})) return false;
	Msg ret(std::move(queue.front()));
	queue.pop();
	_.unlock();
	fn(std::move(ret));
	return true;
}

template<typename Msg, typename QueueImpl>
template<typename Fn>
inline void MsgQueue<Msg, QueueImpl>::modifyQueue(Fn fn) {
	Sync _(lock);
	fn(queue);
	condvar.notify_one();
}

template<typename Msg, typename QueueImpl>
inline void MsgQueue<Msg, QueueImpl>::clear() {
	Sync _(lock);
	queue = QueueImpl();
}

}
#endif
