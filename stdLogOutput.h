


#ifndef _ONDRA_SHARED_STDLOGOUTPUT_H_23312319080809
#define _ONDRA_SHARED_STDLOGOUTPUT_H_23312319080809

#include <ctime>
#include <cstddef>
#include <mutex>
#include "logOutput.h"
#include "refcnt.h"


namespace ondra_shared {


///Declaration of function to write to the output (for example to file)
/** @param text contains text to send to the log file
 */
typedef std::function<void(const StrViewA &text)> LogOutputFn;


class StdStreamLogOutputFn {
public:

	StdStreamLogOutputFn(std::ostream &out):out(out) {}


	void operator()(const StrViewA &line) {
		out << line << std::endl;
	}

private:
	std::ostream &out;
};


template<typename OutputFn>
class StdLogProviderFactory: public RefCntObj, public AbstractLogProviderFactory {
public:
	StdLogProviderFactory(const OutputFn &fn, LogLevel level):fn(fn),enabledLevel(level) {}

	virtual PLogProvider create();


	void sendToLog(const StrViewA &line) {
		std::lock_guard<std::mutex> _(lock);
		fn(line);
	}


	void setEnabledLogLevel(LogLevel lev) {
		enabledLevel = lev;
	}

	bool isLogLevelEnabled(LogLevel lev) const {
		return lev >= enabledLevel;
	}

protected:
	OutputFn fn;
	std::mutex lock;
	LogLevel enabledLevel;

};

template<typename OutputFn>
using PStdLogProviderFactory = RefCntPtr<StdLogProviderFactory<OutputFn> >;


template<typename OutputFn>
class StdLogProvider: public AbstractLogProvider {
public:

	typedef PStdLogProviderFactory<OutputFn> PFactory;

	StdLogProvider(const PFactory &shared):shared(shared) {}
	StdLogProvider(const StdLogProvider &other, StrViewA ident)
		:ident(other.ident+"]["+ident)
		,shared(other.shared) {}

	virtual bool start(LogLevel level, MutableStrViewA &buffer);
	virtual void sendBuffer(MutableStrViewA &text);
	virtual void commit(const MutableStrViewA &text);
	virtual PLogProvider newSection(const StrViewA &ident) ;
	virtual void setProgress(float progressVal, int expectedCycles) ;


protected:
	std::vector<char> buffer;
	std::string ident;
	PFactory shared;


	void finishBuffer(const MutableStrViewA &b);
	void prepareBuffer(MutableStrViewA &b);


};

template<typename OutputFn>
inline PLogProvider StdLogProviderFactory<OutputFn>::create() {
	return PLogProvider(new StdLogProvider<OutputFn>(this));
}

template<typename OutputFn>
inline PLogProvider StdLogProvider<OutputFn>::newSection(
		const StrViewA& ident) {
	return PLogProvider(new StdLogProvider(*this, ident));
}

template<typename OutputFn>
inline void StdLogProvider<OutputFn>::setProgress(float , int ) {
	//not implemented
}


template<typename OutputFn>
inline bool StdLogProvider<OutputFn>::start(LogLevel level, MutableStrViewA& b) {

	if (shared->isLogLevelEnabled(level)) {

		static std::atomic<unsigned int> threadCounter;
		static thread_local unsigned int curThreadId = ++threadCounter;


		buffer.clear();

		auto wrfn = [&](char c) { buffer.push_back(c);};
		auto wrstr = [&](StrViewA c) { for (char x:c) buffer.push_back(x);};

		std::time_t now;
		std::tm tinfo;
		std::time(&now);
		gmtime_r(&now, &tinfo);

		unsignedToString(tinfo.tm_year, wrfn, 10,4);
		buffer.push_back('-');
		unsignedToString(tinfo.tm_mon, wrfn, 10,2);
		buffer.push_back('-');
		unsignedToString(tinfo.tm_mday, wrfn, 10,2);
		buffer.push_back(' ');
		unsignedToString(tinfo.tm_hour, wrfn, 10,2);
		buffer.push_back(':');
		unsignedToString(tinfo.tm_min, wrfn, 10,2);
		buffer.push_back(':');
		unsignedToString(tinfo.tm_sec, wrfn, 10,2);
		wrstr(" ");

		switch (level) {
		case LogLevel::fatal: wrstr("FATAL");break;
		case LogLevel::error: wrstr("Error");break;
		case LogLevel::warning: wrstr("Warn.");break;
		case LogLevel::note: wrstr("Note ");break;
		case LogLevel::info: wrstr("info ");break;
		case LogLevel::debug: wrstr("debug");break;
		default:
		case LogLevel::progress: wrstr("     ");break;
		}

		buffer.push_back(' ');
		buffer.push_back('[');
		unsignedToString(curThreadId, wrfn, 10,4);
		wrstr(ident);
		wrstr(']');
		wrstr(' ');

		b.data = buffer.data();
		b.length = buffer.size();
		prepareBuffer(b);
		return true;
	} else {
		return false;
	}
}

template<typename OutputFn>
inline void StdLogProvider<OutputFn>::sendBuffer(MutableStrViewA& text) {
	prepareBuffer(text);
}

template<typename OutputFn>
inline void StdLogProvider<OutputFn>::commit(const MutableStrViewA& text) {
	finishBuffer(text);
	shared->sendToLog(StrViewA(buffer.data(), buffer.size()));
	buffer.clear();
}





}


#endif /* _ONDRA_SHARED_STDLOGOUTPUT_H_23312319080809 */
