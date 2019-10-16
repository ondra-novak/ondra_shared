/*
 * linux_waitpid.h
 *
 *  Created on: 16. 10. 2019
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_SRC_SHARED_LINUX_WAITPID_H_6876910645
#define ONDRA_SHARED_SRC_SHARED_LINUX_WAITPID_H_6876910645
#include <signal.h>
#include <mutex>
#include <errno.h>
#include <semaphore.h>
#include "linear_map.h"

namespace ondra_shared {

namespace _details {
class WaitPidSvc {
protected:
	using Sync = std::unique_lock<std::recursive_mutex>;
	using Key = std::pair<int, size_t>;
	using Value = std::pair<sem_t *, int>;
	using Map = linear_map<Key,Value>;
	using Set = linear_set<pid_t>;

	class SignalGuard {
	public:
		SignalGuard(int sig) {
			sigset_t newset;
			sigemptyset(&newset);
			sigaddset(&newset, sig);
			pthread_sigmask(SIG_BLOCK, &newset, &oldset);
		}
		~SignalGuard() {
			pthread_sigmask(SIG_SETMASK, &oldset, NULL);
		}

	protected:
		sigset_t oldset;
	};

public:

	size_t beginWait(pid_t pid, sem_t *mtc) {
		SignalGuard _s(SIGCHLD);
		Sync _(mtx);
		int status = -1;
		pid_t q = waitpid(pid, &status, WNOHANG);
		std::size_t reg = nextReg++;
		if (q) {
			//support for copying the status
			auto iter = map.lower_bound(Key(pid,0));
			if (iter != map.end() && iter->first.first == pid) {
				status = iter->second.second;
			}
			//signal if signaled
			sem_post(mtc);
		}
		map.emplace(Key(pid, reg), Value(mtc, status));
		return reg;
	}
	int endWait(pid_t pid, size_t regId) {
		SignalGuard _s(SIGCHLD);
		Sync _(mtx);
		auto iter = map.find(Key(pid, regId));
		int status = -1;
		if (iter != map.end()) {
			status = iter->second.second;
			map.erase(iter);
		}
		return status;
	}

	int getExitCode(pid_t pid, size_t regId) const {
		SignalGuard _s(SIGCHLD);
		Sync _(mtx);
		auto iter = map.find(Key(pid, regId));
		int ret = -1;
		if (iter != map.end()) {
			ret = iter->second.second;
		}
		return ret;
	}

	void signal_enter() {
		int status;
		Sync _(mtx);
		pid_t p = -1;
		bool exited = false;
		for (auto &r : map) {
			if (r.first.first != p) {
				p = r.first.first;
				status = -1;
				exited = false;
				int st = waitpid(p,&status, WNOHANG);
				if (st < 0) {
					exited = true;
					status = errno;
				} else if (st > 0) {
					exited = true;
				}
			}
			if (exited) {
				r.second.second = status;
				sem_post(r.second.first);
			}
		}
	}

	static WaitPidSvc &getSignleton() {
		static WaitPidSvc instance;
		return instance;
	}

	static void signal_enter_static(int) {
		getSignleton().signal_enter();
		init();
	}

	static void init() {
		signal(SIGCHLD, &signal_enter_static);

	}

protected:
	mutable std::recursive_mutex mtx;
	size_t nextReg = 0;
	Map map;



};

}

///Handles waiting for child process asynchronously
/**
 * When object of this class is created, it starts to wait on exit of specified child.
 * The waiting is done asynchronously - at background.
 *
 * If you need to determine, whether the child processes exited, you can
 * call one of the wait function. You can wait as many as you want. Once the
 * child exits, the object becomes signaled and any wait function immediately returns
 *
 * The object uses Linux's signal SIGCHLD and installs global handler for the purpose.
 * It also needs to remove any blocking of this signal.
 *
 * The object is not considered as MT safe. It becomes immediately signaled, when there
 * is no such child running or pending. There can be also multiple WaitPid instances
 * for the single child. You can also copy the WaitPid instance while the signaled
 * state and status is also copied
 *
 */
class WaitPid {
public:
	///Constructs WaitPid object.
	/**
	 * @param pid pid of the child process.
	 *
	 * @note if you constructs the very first instance, the signal handler is
	 * also installed
	 */
	WaitPid(pid_t pid):svc(_details::WaitPidSvc::getSignleton()),pid(pid) {
		sem_init(&sm,0,0);
		_details::WaitPidSvc::init();
		reg = svc.beginWait(pid, &sm);
	}
	///Destructor
	/** Stops waiting for the process */
	~WaitPid() {
		svc.endWait(pid,reg);
		sem_close(&sm);
	}
	///Returns exit code of exited child
	/**
	 * @return exit code. If the child did not exited yet, not exists, or other
	 * error happened, the function returns -1. Other value is similar to status
	 * value returned by wait, wait2 or waitpid, etc. You can use WEXITSTATUS, etc.
	 */
	int getExitCode() const {
		return svc.getExitCode(pid, reg);
	}
	///Determines, whether the child processes exited
	/**
	 * @retval true exited
	 * @retval false still running
	 */
	bool trywait() const {
		if (sem_trywait(&sm) == 0) {
			sem_post(&sm);
			return true;
		} else {
			return false;
		}
	}
	///Waits for child exit
	/** This is equivalent to wait() function
	 *
	 * @retval true success
	 * @retval false failure. Standard errno specifies reason. EINTR is sanitized, you
	 * don't need to check it
	 */
	bool wait() const {
		while (sem_wait(&sm) != 0) {
			if (errno != EINTR) return false;
		}
		sem_post(&sm);
		return true;
	}
	///Waits for specified duration
	/**
	 * @param dur specifies duration
	 * @retval true success
	 * @retval false failure or timeout. Standard errno specifies reason. EINTR is sanitized, you
	 * don't need to check it.
	 */

	template<typename Duration>
	bool wait_for(const Duration &dur) const {
		using namespace std::chrono;
		auto tp = std::chrono::system_clock::now() + dur;
		auto secs = time_point_cast<seconds>(tp);
		auto ns = time_point_cast<nanoseconds>(tp) -
		             time_point_cast<nanoseconds>(secs);

		timespec tmout{secs.time_since_epoch().count(), ns.count()};
		while (sem_timedwait(&sm, &tmout) != 0) {
			if (errno != EINTR) return false;
		}
		sem_post(&sm);
		return true;
	}
	///Creates a copy
	WaitPid(const WaitPid &other):WaitPid(other.pid) {}
protected:
	mutable sem_t sm;
	_details::WaitPidSvc &svc;
	size_t reg;
	pid_t pid;
};



}



#endif /* ONDRA_SHARED_SRC_SHARED_LINUX_WAITPID_H_6876910645 */
