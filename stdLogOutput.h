


#ifndef _ONDRA_SHARED_STDLOGOUTPUT_H_23312319080809
#define _ONDRA_SHARED_STDLOGOUTPUT_H_23312319080809

#include <ctime>
#include <cstddef>
#include <mutex>
#include "logOutput.h"
#include "refcnt.h"


namespace ondra_shared {



class StdLogProviderFactory: public RefCntObj, public AbstractLogProviderFactory {
public:
	StdLogProviderFactory(LogLevel level = LogLevel::info):enabledLevel(level) {}

	virtual PLogProvider create() override;

	virtual void writeToLog(const StrViewA &line, const std::time_t &) {
		std::cerr << line << std::endl;
	}

	void sendToLog(const StrViewA &line, const std::time_t &time) {
		std::lock_guard<std::mutex> _(lock);
		writeToLog(line, time);
	}


	void setEnabledLogLevel(LogLevel lev) {
		enabledLevel = lev;
	}

	virtual bool isLogLevelEnabled(LogLevel lev) const override {
		return lev >= enabledLevel;
	}



protected:
	std::mutex lock;
	LogLevel enabledLevel;

};

using PStdLogProviderFactory = RefCntPtr<StdLogProviderFactory>;


class StdLogProvider: public AbstractLogProvider {
public:

	typedef PStdLogProviderFactory PFactory;

	StdLogProvider(const PFactory &shared):shared(shared) {}
	StdLogProvider(const StdLogProvider &other, StrViewA ident)
		:ident((other.ident+"][").append(ident.data, ident.length))
		,shared(other.shared) {}

	virtual bool start(LogLevel level, MutableStrViewA &buffer);
	virtual void sendBuffer(MutableStrViewA &text);
	virtual void commit(const MutableStrViewA &text);
	virtual PLogProvider newSection(const StrViewA &ident) ;
	virtual void setProgress(float progressVal, int expectedCycles) ;


	virtual bool isLogLevelEnabled(LogLevel level) const {
		return shared->isLogLevelEnabled(level);
	}
protected:
	std::vector<char> buffer;
	std::string ident;
	PFactory shared;
	time_t lastTime;


	void finishBuffer(const MutableStrViewA &b);
	void prepareBuffer(MutableStrViewA &b);


};

inline PLogProvider StdLogProviderFactory::create() {
	return PLogProvider(new StdLogProvider(this));
}

inline PLogProvider StdLogProvider::newSection( const StrViewA& ident) {
	return PLogProvider(new StdLogProvider(*this, ident));
}

inline void StdLogProvider::setProgress(float , int ) {
	//not implemented
}


inline bool StdLogProvider::start(LogLevel level, MutableStrViewA& b) {

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

		unsignedToString(tinfo.tm_year+1900, wrfn, 10,4);
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
		wrstr("] ");

		b = MutableStrViewA(buffer.data(), buffer.size());
		prepareBuffer(b);
		lastTime = now;
		return true;
	} else {
		return false;
	}
}

inline void StdLogProvider::sendBuffer(MutableStrViewA& text) {
	prepareBuffer(text);
}

inline void StdLogProvider::commit(const MutableStrViewA& text) {
	finishBuffer(text);
	shared->sendToLog(StrViewA(buffer.data(), buffer.size()), lastTime);
	buffer.clear();
}


inline void StdLogProvider::finishBuffer(const MutableStrViewA& b) {
	auto offset = b.data - buffer.data();
	buffer.resize(b.length+offset);
}

inline void StdLogProvider::prepareBuffer(MutableStrViewA& b) {
	finishBuffer(b);
	auto offset = buffer.size();
	buffer.resize(offset*3/2+400);
	b = MutableStrViewA(buffer.data()+offset, buffer.size()-offset);
}

}
#endif /* _ONDRA_SHARED_STDLOGOUTPUT_H_23312319080809 */
