
///@file simple thread pool implementation
#ifndef __ONDRA_SHARED_THREAD_POOL_H_1289EOAWDH230EFJ390TFE
#define __ONDRA_SHARED_THREAD_POOL_H_1289EOAWDH230EFJ390TFE

#include <condition_variable>
#include <queue>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ondra_shared {

///simple thread pool
class thread_pool {
public:

    ///Creates thread pool
    /**
     * @param thrcnt count of threads
     */
    explicit thread_pool(int thrcnt);
    ///Destructs the thread pool
    /**
     * Destructor synchronously ends all running threads. There is implicit join operation
     * Ensure, that nothing is added during destruction (as the destruction is not MT safe)
     */
    ~thread_pool();

    ///Disabled copy constructor
    /**
     * You cannot copy thread_pool
     */
    thread_pool(const thread_pool &other) = delete;
    ///Disabled assignment
    /**
     * You cannot assign to thread_pool
     */
    thread_pool &operator=(const thread_pool &other) = delete;

    ///Enqueue and execute a function on thread pool
    /**
     * Enqueues and eventually executes function on thread pool
     *
     * @param fn function to run
     */
    template<typename Fn>
    void run(Fn &&fn);

    ///Enqueue and execute a function on thread pool
    /**
     * Enqueues and eventually executes function on thread pool
     *
     * @param fn function to run
     */
    template<typename Fn>
    void operator>>(Fn &&fn);

    ///Clears all pending executions
    /**
     * Removes all enqueued functions. Functions already in processing are not interrupted
     * and they are eventually finished.
     */
    void clear();

    ///Stops the thread pool
    /**
     * Removes all enqueued functions. Finishes all running functions and joins all threads.
     * Once the thread_pool is stopped, it cannot be restarted. You need to recreate it
     */
    void stop();
    
    ///Stops (non-blocking) the thread pool without waiting for join threads
    /**
     * it only flags thread_pool stopped, all threads finish its works and ends, however the function
     * doesn't join threads, so function doesn't block. (Threads are eventually joined in destructor)
     */
    void stop_nb();
    
    ///Starts one thread adding it to the pool
    /**
     * @return count of running threads
     *
     * @note you cannot add threads on stopped thread pool;
     */
    std::size_t start_thread();

    ///Stops the one thread
    /**
     * Function enqueues special action, which causes that one thread is stopped. Note that
     * the action is enqueued so if there are actions before in the queue, they are
     * processed first. There is no notification about when the thread was stopped.
     *
     * @note If you stop more threads than currently running, remaining requests stays enqueued,
     * and are able to kill the first thread to be started
     */
    void stop_thread();

    ///namespace to work with current thread pool (inside of managed thread)
    struct current {
        ///Determines, whether current thread pool has been stopped
        /**
         * It is intended to interrupt a long-running operation in a situation
         * where the thread_pool has been stopped, so that the operation
         * does not block its destruction
         * @retval false thread_pool isn't stopped
         * @retval true thread_pool is stopped
         *
         * @note If called from non-managed thread, return true (because there is no thread pool)
         */
        static bool is_stopped();
        ///Allows to stop (non-blocking) thread pool from inside (from managed thread)
        /**
         * Similar to thread_pool::stop_nb() if called from managed thread
         *
         * @note If called from non-managed thread, does nothing (similar to stop already stopped thread_pool)
         */
        static void stop_nb();
        ///The same as thread_pool::clear() but from managed thread
        /**
         * @note If called from non-managed thread, does nothing (similar to clear empty thread_pool)
         */
        static void clear();

        ///Runs another function in the current thread_pool
        /**
         * @param fn function to run
         * @retval true function started
         * @retval false called from non-managed thread, so function cannot be called
         */
        template<typename Fn>
        static bool run(Fn &&fn);

        ///Runs another function in the current thread_pool
        /**
         * @param fn function to run
         * @retval true function started
         * @retval false called from non-managed thread, so function cannot be called
         */
        template<typename Fn>
        bool operator>>(Fn &&fn) {
            return run(std::forward<Fn>(fn));
        }

        static bool stop_thread();

        static bool start_thread();


    };


    ///Determines, whether thread was stopped
    /**
     * This can be useful, if the running function can interrupt its execution in case, that
     * thread_pool was stopped, so it can finish quickly and reduce blocking time
     *
     * @retval true thread_pool is stopped
     * @retval false thread_pool isn't stopped
     *
     * @note if called outside of thread_pool, always returns true.
     */
    bool is_stopped();

protected:

    class action_t {
    public:
        virtual ~action_t() = default;
        virtual void run() noexcept = 0;
    };


    template<typename Fn>
    class action_fn_t: public action_t {
    public:
        action_fn_t(Fn &&fn):fn(std::forward<Fn>(fn)) {}
        virtual void run() noexcept {
            fn();
        }
    protected:
        Fn fn;
    };

    using paction_t = std::unique_ptr<action_t>;
    using queue_t = std::queue<paction_t>;
    using thrlst_t = std::vector<std::thread>;

    queue_t _q;
    mutable std::mutex _m;
    std::condition_variable _c;
    thrlst_t _l;
    bool _s;


    void worker();


    static thread_pool * &get_current_ptr();

};

inline thread_pool::thread_pool(int thrcnt)
:_s(false)
{
    for (int i = 0; i < thrcnt; i++) {
        _l.emplace_back([this]{
            worker();
        });
    }
}

inline thread_pool::~thread_pool() {
    stop();
}

template<typename Fn>
inline void thread_pool::run(Fn &&fn) {
    std::unique_lock<std::mutex> _(_m);
    _q.push(std::make_unique<action_fn_t<Fn> >(std::forward<Fn>(fn)));
    _c.notify_one();

}

template<typename Fn>
inline void thread_pool::operator >>(Fn &&fn) {
    run(std::forward<Fn>(fn));
}

inline void thread_pool::clear() {
    std::unique_lock<std::mutex> _(_m);
    _q = queue_t();
}

inline void thread_pool::stop_nb() {
    std::unique_lock<std::mutex> _(_m);
    _s = true;
    _c.notify_all();
}

inline void thread_pool::stop() {
    thrlst_t tmp;
    {
        std::unique_lock<std::mutex> _(_m);
        std::swap(_l, tmp);
        _s = true;
        _c.notify_all();
    }
    for (auto &x: tmp) {
        x.join();
    }
}

inline void thread_pool::worker() {
    get_current_ptr() = this;
    std::unique_lock<std::mutex> _(_m);
    while (!_s) {
        _c.wait(_,[this]{return _s || !_q.empty();});
        if (!_s) {
            auto f = std::move(_q.front());
            if (f) {
                _q.pop();
                _.unlock();
                f->run();
                _.lock();
            } else {
                break;
            }
        }
    }
}

inline bool thread_pool::is_stopped() {
    return _s;
}

inline std::size_t thread_pool::start_thread() {
    std::unique_lock _(_m);
    auto itr = std::remove_if(_l.begin(), _l.end(), [](std::thread &t) {
       if (t.joinable()) {
           t.join();
           return true;
       } else {
           return false;
       }
    });
    _l.erase(itr, _l.end());
    _l.push_back(std::thread([=]{
        worker();
    }));
    return _l.size();
}

inline void thread_pool::stop_thread() {
    std::unique_lock _(_m);
    _q.push(nullptr);
}

inline thread_pool*& thread_pool::get_current_ptr() {
   static thread_local thread_pool *_current;
   return _current;
}

template<typename Fn>
inline bool thread_pool::current::run(Fn &&fn) {
    thread_pool *inst = get_current_ptr();
    if (inst) {
        inst->run(std::forward<Fn>(fn));
        return true;
    } else {
        return false;
    }

}

inline bool thread_pool::current::is_stopped() {
    thread_pool *inst = get_current_ptr();
    return !inst || inst->is_stopped();
}

inline void thread_pool::current::stop_nb() {
    thread_pool *inst = get_current_ptr();
    if (inst) inst->stop_nb();
}

inline void thread_pool::current::clear() {
    thread_pool *inst = get_current_ptr();
    if (inst) inst->clear();
}

inline bool thread_pool::current::stop_thread() {
    thread_pool *inst = get_current_ptr();
    if (inst) {
        inst->stop_thread();
        return true;
    } else {
        return false;
    }

}

static bool thread_pool::current::start_thread() {
    thread_pool *inst = get_current_ptr();
    if (inst) {
        inst->start_thread();
        return true;
    } else {
        return false;
    }
}


}
#endif /* __ONDRA_SHARED_THREAD_POOL_H_1289EOAWDH230EFJ390TFE */

