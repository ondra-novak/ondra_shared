
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
    thread_pool(int thrcnt);
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

    ///Retrieves pointer to current thread pool instance
    /**
     * Function returns a valid pointer only if it is called inside of the pool's thread.
     * This allows the function to enqueue additional actions to the same thread pool.
     *
     * Function returns null outside of thread pool
     *
     * @return pointer to current thread pool (inside) or nullptr (outside)
     */
    static thread_pool *get_current();

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
    static bool is_stopped();

    ///Determines. whether specified thread_pool is stopped
    /**
     * @param pool pool to test
     * @retval true thread_pool is stopped
     * @retval false thread_pool isn't stopped
     */
    static bool is_stopped(const thread_pool &pool);


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
        _l.push_back(std::thread([this]{
            worker();
        }));
    }
}

inline thread_pool::~thread_pool() {
    stop();
}

template<typename Fn>
inline void thread_pool::run(Fn &&fn) {
    std::unique_lock _(_m);
    _q.push(std::make_unique<action_fn_t<Fn> >(std::forward<Fn>(fn)));
    _c.notify_one();

}

template<typename Fn>
inline void thread_pool::operator >>(Fn &&fn) {
    run(std::forward<Fn>(fn));
}

inline void thread_pool::clear() {
    std::unique_lock _(_m);
    _q = queue_t();
}

inline void thread_pool::stop() {
    thrlst_t tmp;
    {
        std::unique_lock _(_m);
        std::swap(_l, tmp);
        _s = true;
        _c.notify_all();
    }
    for (auto &x: tmp) {
        x.join();
    }
}

inline thread_pool* thread_pool::get_current() {
    return get_current_ptr();
}

inline void thread_pool::worker() {
    get_current_ptr() = this;
    std::unique_lock _(_m);
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
    auto s = get_current();
    return !s || s->_s;
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

inline bool thread_pool::is_stopped(const thread_pool &pool) {
    std::unique_lock _(pool._m);
    return pool._s;
}

inline thread_pool*& thread_pool::get_current_ptr() {
   static thread_local thread_pool *_current;
   return _current;
}


}
#endif /* __ONDRA_SHARED_THREAD_POOL_H_1289EOAWDH230EFJ390TFE */

