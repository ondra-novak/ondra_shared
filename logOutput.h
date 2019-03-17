#ifndef _ONDRA_SHARED_DEBUGLOG_H_2908332900212092_
#define _ONDRA_SHARED_DEBUGLOG_H_2908332900212092_
#include <algorithm>
#include <memory>
#include <atomic>

#include "stringview.h"
#include "toString.h"
#include "virtualMember.h"


namespace ondra_shared {

typedef MutableStringView<char> MutableStrViewA;

enum class LogLevel {
	///debug level
	/** "processing inner cycle" */
	debug = 0,
	///non debug information
	/** "descriptor ID = 10" */
	info = 1,
	///information about progress
	/** "reading file", "processing input" */
	progress = 2,
	///important non-error information
	/** "file has been truncated" */
	note = 3,
	///warning, information which could be error in some cases
	/** file is read-only, no data has been stored */
	warning = 4,
	///error, which caused, that part of code has not been finished, but application still can continue
	/** "connection lost" */
	error = 5,
	///fatal error which caused, that application stopped
	/** "critical error in the chunk ABC" */
	fatal = 6,
	///logging is complete disabled
	off = 7
};

class AbstractLogProvider;
using PLogProvider = std::unique_ptr<AbstractLogProvider> ;

class AbstractLogProviderFactory {
public:

	///Creates log provider
	/**
	 * @return pointer to log provider
	 */
	virtual PLogProvider create() = 0;


	///access to log provider factory pointer
	static AbstractLogProviderFactory *& getInstance() {
		static AbstractLogProviderFactory *globalFactory = nullptr;

		return globalFactory;
	}


	///Makes this provider as default
	/**
	 * Function makes this provider factory as default log factory. It also
	 * changes log provider of current thread. You need to manually change log provider
	 * in other threads
	 */
	virtual void setDefault();

	///Notifies the log provider (factory) about log-rotate event and requests to reopen log files
	virtual void reopenLogs() {}

	virtual bool isLogLevelEnabled(LogLevel level) const = 0;

	virtual ~AbstractLogProviderFactory() {}

};


class AbstractLogProvider {
public:
	///Start sending a log message
	/**
	 * @param level log level
	 * @param buffer reference to unitialized MutableStringView. Function
	 * fills this object with valid buffer location. Caller then must
	 * fill the buffer with character and call sendBuffer(). To
	 * finish log message, caller has to call commit();
	 */
	virtual bool start(LogLevel level, MutableStrViewA &buffer) = 0;
	///Sends buffer.
	/**
	 * @param text buffer which contains the text. Length of the buffer
	 * must be same as length of the text. It is possible to send
	 * text from different buffer. Function updates object to contain
	 * location of new buffer
	 */
	virtual void sendBuffer(MutableStrViewA &text) = 0;
	///Commits the log essage
	/**
	 */
	virtual void commit(const MutableStrViewA &text) = 0;


	///Creates copy of current log provider for specified part of the code which doesn't execute in the single thread
	/** You attach the log provider to an object and call the log provider
	 *  everytime when something happen with that object. You can also set name
	 *  of the object through the argument.
	 *
	 *  The newly created provider is NOT Multithread Safe. You should always
	 *  wrapp calling with mutexes. However it have to be safe to use both providers (original
	 *  and newly created) in different threads at time
	 */
	virtual PLogProvider newSection(const StrViewA &ident) = 0;


	///Sets the progress value of the current task
	/** The progress must be reported before the particular task starts
	 *
	 * @param progressVal value of the progress at the beginning of the task. The
	 *   value is between 0 (just started) and 1 (completted)
	 * @param expectedCycles expecteded count of cycles. If this value is set
	 * to zero, no subtasks are tracked. If this value is set to one, it expects
	 * thath following subtask is final cycle.
	 */
	virtual void setProgress(float progressVal, int expectedCycles) = 0;


	///Gets current instance of the log provider
	/**
	 * @return reference to poiter which contains the log provider's instance. Note that
	 * function can return nullptr, when the log provider was not created yet. You can
	 * also use the reference to set a new provider
	 *
	 * @note if you need to initialize instance for sending data to the log, use
	 * the function initInstance()
	 */
	static PLogProvider &getInstance() {
		static thread_local PLogProvider curInst(nullptr);
		return curInst;
	}

	///Gets current instance or initializes the instance for the new thread
	/**
	 * @return reference to the log provider's instance. The pointer is initialized
	 * on the first call. However, the function can return nullptr when there
	 * is no active global log provider, or when the log provider fails to
	 * initialize the instance. In the case of nullptr, the logging should
	 * be disabled for the thread
	 */
	static PLogProvider &initInstance() {
		PLogProvider &p = getInstance();
		if (p == nullptr) {
			AbstractLogProviderFactory *f = AbstractLogProviderFactory::getInstance();
			if (f != nullptr)
				p = f->create();
		}
		return p;
	}
	virtual bool isLogLevelEnabled(LogLevel level) const = 0;

	virtual ~AbstractLogProvider() {}
};





class LogWriterFn {
public:

	LogWriterFn(AbstractLogProvider *provider, LogLevel level):provider(provider) {
		enabled = provider && provider->start(level, buffer);
		if (!enabled) buffer = MutableStrViewA(dummybuff, sizeof(dummybuff));
		pos=0;
	}
	void operator()(char c) const {
		buffer[pos] = c;
		pos = (pos + 1) % buffer.length;
		if (pos == 0 && enabled) provider->sendBuffer(buffer);
	}
	void operator()(const StrViewA &c)const {
		auto remain = buffer.length - pos;
		StrViewA part = c.substr(0,remain);
		std::copy(part.data, part.data+part.length, buffer.data+pos);
		pos = (pos + part.length) % buffer.length;
		StrViewA rest = c.substr(part.length);
		if (pos == 0 && enabled) provider->sendBuffer(buffer);
		if (!rest.empty()) operator()(rest);
	}

	~LogWriterFn() {
		if (enabled)
			provider->commit(buffer.substr(0,pos));
	}

	LogWriterFn(LogWriterFn &&other)
		:enabled(other.enabled)
		,provider(other.provider)
		,buffer(other.buffer)
		,pos(other.pos) {
		other.enabled = false;
	}

	bool enabled;
protected:
	AbstractLogProvider *provider;
	mutable MutableStrViewA buffer;
	mutable int pos;
	char dummybuff[32];
};


template<typename WriteFn> void logPrintValue(const WriteFn &wr, StrViewA v) {	wr(v);}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, const char *v) {	wr(StrViewA(v));}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, BinaryView v) {
	for (auto &&c : v) unsignedToString(c,wr,16,2);
}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, unsigned long long v) {unsignedToString(v,wr);}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, unsigned long v) {unsignedToString(v,wr);}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, unsigned int v) {unsignedToString(v,wr);}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, unsigned short v) {unsignedToString(v,wr);}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, signed long long v) {signedToString(v,wr);}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, signed long v) {signedToString(v,wr);}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, signed int v) {signedToString(v,wr);}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, signed short v) {signedToString(v,wr);}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, double v) {floatToString(v,wr,8);}
template<typename WriteFn> void logPrintValue(const WriteFn &wr, float v) {floatToString(v,wr,8);}
template<typename WriteFn, typename T> void logPrintValue(const WriteFn &wr, const std::initializer_list<T> &v) {
	for (auto &&x:v ) {
		wr(" ");
		logPrintValue(wr, x);
	}
}
template<typename WriteFn, typename T> void logPrintValue(const WriteFn &wr, T *v) {
	std::uintptr_t w = reinterpret_cast<std::uintptr_t>(v);
	unsignedToString(w,wr,16,sizeof(w)*2);
}

namespace _logDetails {

	static inline void renderNthArg(LogWriterFn &, unsigned int ) {}

	template<typename T, typename... Args>
	static inline void renderNthArg(LogWriterFn &wr, unsigned int index, T &&arg1, Args &&... args) {
		if (index == 1) {
			logPrintValue(wr, std::forward<T>(arg1));
		} else {
			renderNthArg(wr, index-1, std::forward<Args>(args)...);
		}
	}
}


template<typename Provider, typename... Args>
void logPrint(Provider &lp, LogLevel level, const StrViewA &pattern, Args&&... args) {

	using namespace _logDetails;

	if (lp == nullptr) return;

	AbstractLogProvider *p = lp.get();
	LogWriterFn wr(p,level);
	if (wr.enabled) {

		enum CurMode {
			readChar,
			readStartArg,
			readArgIndex,
			readParIndex
		};

		CurMode md = readChar;
		unsigned int curIndex = 0;
		for (auto c : pattern) {
			switch (md) {
			case readChar:
				if (c == '$') md = readStartArg;
				else wr(c);
				break;
			case readStartArg:
				if (isdigit(c)) {
					curIndex = c - '0';
					md = readArgIndex;
				} else if (c == '(') {
					md = readParIndex;
				} else {
					wr(c);
					md = readChar;
				}
				break;
			case readArgIndex:
				if (isdigit(c)) {
					curIndex = curIndex * 10 + (c - '0');
				} else {
					renderNthArg(wr, curIndex, std::forward<Args>(args)...);
					md = readChar;
					wr(c);
				}
				break;
			case readParIndex:
				if (isdigit(c)) {
					curIndex = curIndex * 10 + (c - '0');
				} else if (c == ')') {
					renderNthArg(wr, curIndex, std::forward<Args>(args)...);
					md = readChar;
				} else {
					wr("$(");
					unsignedToString(curIndex, wr,10,1);
					md = readChar;
					wr(c);
				}
				break;
			}
		}
		if (md == readArgIndex) {
			renderNthArg(wr, curIndex, std::forward<Args>(args)...);
		}
	}


};


///Object purposed to perform log output to special sections disconnected to current thread
/**
 * By default global log output is managed per thread. Many object often live
 * in many threads and also often we need to log these object into separated log section
 * regardless on which thread operation is logged.
 *
 * The LogObject created additional log provider from existing or global log provider
 * and registers it under new name of section.
 *
 * Log output for the LogObject must be performed through the object, you cannot use
 * global logging unless you use setCurrent or swapCurrent
 *
 * @note LogObject cannot be copied. You can only move the object.
 */
template<typename Backend>
class LogObjectT {

		class WrFn {
		public:
			mutable std::string s;
			void operator()(char c) const {s.push_back(c);}
			void operator()(StrViewA str) const {s.append(str.data, str.length);}
		};


	public:


		///Log to the output a fatal error
		/**
		 * @param pattern pattern, use $1...$n or $(1)...$(n) as placeholders
		 * @param args arguments for placeholders
		 */
		template<typename... Args>
		void fatal(const StrViewA &pattern, Args&&... args) const {
			logPrint(lp,LogLevel::fatal, pattern, std::forward<Args>(args)...);
		}
		///Log to the output a warning
		/**
		 * @param pattern pattern, use $1...$n or $(1)...$(n) as placeholders
		 * @param args arguments for placeholders
		 */
		template<typename... Args>
		void warning(const StrViewA &pattern, Args&&... args)const  {
			logPrint(lp,LogLevel::warning, pattern, std::forward<Args>(args)...);
		}
		///Log to the output an error
		/**
		 * @param pattern pattern, use $1...$n or $(1)...$(n) as placeholders
		 * @param args arguments for placeholders
		 */
		template<typename... Args>
		void error(const StrViewA &pattern, Args&&... args) const {
			logPrint(lp,LogLevel::error, pattern, std::forward<Args>(args)...);
		}
		///Log to the output a note
		/**
		 * @param pattern pattern, use $1...$n or $(1)...$(n) as placeholders
		 * @param args arguments for placeholders
		 */
		template<typename... Args>
		void note(const StrViewA &pattern, Args&&... args) const {
			logPrint(lp,LogLevel::note, pattern, std::forward<Args>(args)...);
		}
		///Log to the output a prohress
		/**
		 * @param pattern pattern, use $1...$n or $(1)...$(n) as placeholders
		 * @param args arguments for placeholders
		 */
		template<typename... Args>
		void progress(const StrViewA &pattern, Args&&... args) const {
			logPrint(lp,LogLevel::progress, pattern, std::forward<Args>(args)...);
		}
		///Log to the output an info
		/**
		 * @param pattern pattern, use $1...$n or $(1)...$(n) as placeholders
		 * @param args arguments for placeholders
		 */
		template<typename... Args>
		void info(const StrViewA &pattern, Args&&... args) const {
			logPrint(lp,LogLevel::info, pattern, std::forward<Args>(args)...);
		}

		///Log to the output a debug
		/**
		 * @param pattern pattern, use $1...$n or $(1)...$(n) as placeholders
		 * @param args arguments for placeholders
		 */
		template<typename... Args>
		void debug(const StrViewA &pattern, Args&&... args) const {
			logPrint(lp,LogLevel::debug, pattern, std::forward<Args>(args)...);
		}


		///Swap this provider with thread local provider
		/** Function swaps thread local provider with the provider
		 * active on this object. This allows to redirect thread local log
		 * output to the log provider assigned to this instance of LogObject. However
		 * it also makes that log provider inacccesible from this instance.
		 *
		 * To return back to normal, call the function swap() again
		 *
		 * @code
		 * LogObject lg(...);
		 *
		 * lg.swap();
		 * runComplexOperation();
		 * lg.swap();
		 * @endcode
		 *
		 * @note Above code is not exception safe. To achieve exception safety, you
		 * should use LogObject::Swap instead
		 */
		void swap() {
			PLogProvider &cur = AbstractLogProvider::getInstance();
			std::swap(cur,lp);
		}

		///Creates section where specified LogObject has the log provider swapped with thread local provider
		/** for more information, see swap()
		 *
		 * @code
		 *
		 * @code
		 * LogObject lg(...);
		 *
		 * {
		 * 	LogObject::Swap swap(lg);
		 * 	runComplexOperation();
		 * } //swapped back here
		 * @endcode
		 */
		class Swap {
		public:
			Swap(LogObjectT &h):h(h) {h.swap();}
			~Swap() {h.swap();}
		protected:
			LogObjectT &h;
		};


		///Create log object using global log provider
		/**
		 * @param v identification of the section. It is required to have
		 * function that is able to convert the section identification to string
		 */
		template<typename T>
		LogObjectT(const T &v)
			 {

			AbstractLogProvider *clp = AbstractLogProvider::initInstance().get();
			initSection(clp, v);
		}

		///Create log object using a specified log provider
		/**
		 * @param x source log object
		 * @param v identification of the section. It is required to have
		 * function that is able to convert the section identification to string
		 */
		template<typename T>
		LogObjectT(const LogObjectT &x, const T &v)
		{

			initSection(x.lp, v);
		}

		///Create unitialized log object
		/**Using such log object doesn't produce any output, however you can initialize
		 * the log provider later */
		LogObjectT() {}


		template<typename T>
		LogObjectT( AbstractLogProvider &lp, const T &v)
		{

			initSection(&lp, v);
		}

		AbstractLogProvider *getProvider() const {
			return lp.get();
		}


		AbstractLogProvider &operator*() const {
			return *lp.get();
		}


		bool isLogLevelEnabled(LogLevel level) const {
			return lp != nullptr && lp->isLogLevelEnabled(level);
		}



	protected:
		Backend lp;

		template<typename T>
		void initSection(AbstractLogProvider *clp, const T &v) {
			if (clp != nullptr) {
				WrFn wr;
				logPrintValue(wr,v);
				lp = clp->newSection(wr.s);
			}
		}
	};


	template<typename... Args>
	void logFatal(const StrViewA &pattern, Args&&... args) {
		logPrint(AbstractLogProvider::initInstance(),LogLevel::fatal, pattern, std::forward<Args>(args)...);
	}
	template<typename... Args>
	void logWarning(const StrViewA &pattern, Args&&... args) {
		logPrint(AbstractLogProvider::initInstance(),LogLevel::warning, pattern, std::forward<Args>(args)...);
	}
	template<typename... Args>
	void logError(const StrViewA &pattern, Args&&... args) {
		logPrint(AbstractLogProvider::initInstance(),LogLevel::error, pattern, std::forward<Args>(args)...);
	}
	template<typename... Args>
	void logNote(const StrViewA &pattern, Args&&... args) {
		logPrint(AbstractLogProvider::initInstance(),LogLevel::note, pattern, std::forward<Args>(args)...);
	}
	template<typename... Args>
	void logProgress(const StrViewA &pattern, Args&&... args) {
		logPrint(AbstractLogProvider::initInstance(),LogLevel::progress, pattern, std::forward<Args>(args)...);
	}
	template<typename... Args>
	void logInfo(const StrViewA &pattern, Args&&... args) {
		logPrint(AbstractLogProvider::initInstance(),LogLevel::info, pattern, std::forward<Args>(args)...);
	}
	template<typename... Args>
	void logDebug(const StrViewA &pattern, Args&&... args) {
		logPrint(AbstractLogProvider::initInstance(),LogLevel::debug, pattern, std::forward<Args>(args)...);
	}

	inline void AbstractLogProviderFactory::setDefault() {
		getInstance() = this;
		AbstractLogProvider::getInstance() = create();
	}

	class LogLevelToStrTable {
	public:
		LogLevelToStrTable() {
			static std::pair<StrViewA,LogLevel> levelMapDef[] = {
					{"debug", LogLevel::debug},
					{"info",LogLevel::info},
					{"progress",LogLevel::progress},
					{"note",LogLevel::note},
					{"warning",LogLevel::warning},
					{"error",LogLevel::error},
					{"fatal",LogLevel::fatal},
					{"off",LogLevel::off}
			};
			levelMap = StringView<std::pair<StrViewA,LogLevel> >(levelMapDef, sizeof(levelMapDef)/sizeof(levelMapDef[0]));
		}

		LogLevel fromString(StrViewA s, LogLevel def = LogLevel::off) const {
			for (auto &&x:levelMap) if (x.first == s) return x.second;
			return def;
		}
		StrViewA toString(LogLevel l) const {
			for (auto &&x:levelMap) if (x.second == l) return x.first;
			return StrViewA();
		}

	protected:
		StringView<std::pair<StrViewA,LogLevel> >levelMap;
	};


	template<const char *&prefix>
	class TaskCounter {
	public:
		TaskCounter () {
			static std::atomic<unsigned int> counter(0);
			curValue = ++counter;
		}

	unsigned int curValue;
	};

	template<typename WriteFn, const char *&prefix>
	void logPrintValue(const WriteFn &wr, const TaskCounter<prefix> &t) {
		wr(prefix);
		wr(':');
		wr(t.curValue);
	}


	typedef LogObjectT<PLogProvider> LogObject;
	typedef LogObjectT<std::shared_ptr<AbstractLogProvider> > SharedLogObject;



	inline std::string what(const std::exception_ptr &e) {
		try {
			std::rethrow_exception(e);
		} catch (const std::exception &e) {
			return e.what();
		} catch (...) {
			return "unknown exception";
		}
	}


}


#endif
