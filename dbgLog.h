#ifndef _ONDRA_SHARED_DEBUGLOG_H_2908332900212092_
#define _ONDRA_SHARED_DEBUGLOG_H_2908332900212092_
#include <algorithm>
#include <memory>

#include "stringview.h"
#include "toString.h"


namespace ondra_shared {

typedef MutableStringView<char> MutableStrViewA;

enum class LogLevel {
	///debug level
	/** "processing inner cycle" */
	debug,
	///non debug information
	/** "descriptor ID = 10" */
	info,
	///information about progress
	/** "reading file", "processing input" */
	progress,
	///important non-error information
	/** "file has been truncated" */
	note,
	///warning, information which could be error in some cases
	/** file is read-only, no data has been stored */
	warning,
	///error, which caused, that part of code has not been finished, but application still can continue
	/** "connection lost" */
	error,
	///fatal error which caused, that application stopped
	/** "critical error in the chunk ABC" */
	fatal
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


	///Push current thread identification
	/** useful when tracking task accross threads
	 *
	 * Current identification is handled in the stack. You need to pushIdent
	 * when the task has assigned the thread, and you need to pupIdent, when
	 * the current task is leaving the thread.
	 *
	 * It is better to handle this using class LogIdent
	 */
	virtual void pushIdent(const StrViewA &ident) = 0;

	///Removes ident from the internal stack
	virtual void popIdent() = 0;


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
		static thread_local PLogProvider curInst;
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
};



class LogWriterFn {
public:

	LogWriterFn(AbstractLogProvider *provider, LogLevel level):provider(provider) {
		enabled = provider && provider->start(level, buffer);
		if (!enabled) buffer = MutableStrViewA(dummybuff, sizeof(dummybuff));
		pos=0;
	}
	void operator()(char c) {
		buffer[pos] = c;
		pos = (pos + 1) % buffer.length;
		if (pos == 0 && enabled) provider->sendBuffer(buffer);
	}
	void operator()(const StrViewA &c) {
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
	MutableStrViewA buffer;
	int pos;
	char dummybuff[32];
};


template<typename WriteFn> void logPrintValue(const WriteFn &wr, StrViewA v) {	wr(v);}
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
template<typename WriteFn, typename T> void logPrintValue(const WriteFn &wr, T *v) {
	std::uintptr_t w = reinterpret_cast<std::uintptr_t>(v);
	unsignedToString(w,wr,16,sizeof(w)*2);
}

namespace _logDetails {

	static inline void renaderNthArg(LogWriterFn &wr, unsigned int index) {}

	template<typename T, typename... Args>
	static inline void renderNthArg(LogWriterFn &wr, unsigned int index, T &&arg1, Args &&... args) {
		if (index == 1) {
			logPrintValue(wr, std::forward<T>(arg1));
		} else {
			renderNthArg(wr, index-1, std::forward<Args...>(args...));
		}
	}
}

template<typename... Args>
void logPrint(LogLevel level, const StrViewA &pattern, Args&&... args) {

	using namespace _logDetails;

	PLogProvider &lp = AbstractLogProvider::initInstance();
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
	class LogIdent {

		class WrFn {
		public:
			mutable std::string s;
			void operator()(char c) const {s.push_back(c);}
			void operator()(StrViewA str) const {s.append(str.data, str.length);}
		};


	public:



		template<typename T>
		LogIdent(const T &v) {
			PLogProvider &lp = AbstractLogProvider::initInstance();
			ref = lp.get();
			if (ref == nullptr) return;
			WrFn wr;
			logPrintValue(wr,v);
			lp->pushIdent(wr.s);
		}

		~LogIdent() {
			PLogProvider &lp = AbstractLogProvider::initInstance();
			if (lp.get() == ref) {
				ref->popIdent();
			}
		}

	protected:
		AbstractLogProvider *ref;
	};


	template<typename... Args>
	void logFatal(const StrViewA &pattern, Args&&... args) {
		logPrint(LogLevel::fatal, pattern, std::forward<Args...>(args...));
	}
	template<typename... Args>
	void logWarning(const StrViewA &pattern, Args&&... args) {
		logPrint(LogLevel::warning, pattern, std::forward<Args...>(args...));
	}
	template<typename... Args>
	void logError(const StrViewA &pattern, Args&&... args) {
		logPrint(LogLevel::error, pattern, std::forward<Args...>(args...));
	}
	template<typename... Args>
	void logNote(const StrViewA &pattern, Args&&... args) {
		logPrint(LogLevel::note, pattern, std::forward<Args...>(args...));
	}
	template<typename... Args>
	void logProgress(const StrViewA &pattern, Args&&... args) {
		logPrint(LogLevel::progress, pattern, std::forward<Args...>(args...));
	}
	template<typename... Args>
	void logInfo(const StrViewA &pattern, Args&&... args) {
		logPrint(LogLevel::info, pattern, std::forward<Args...>(args...));
	}
	template<typename... Args>
	void logDebug(const StrViewA &pattern, Args&&... args) {
		logPrint(LogLevel::debug, pattern, std::forward<Args...>(args...));
	}

}



#endif
