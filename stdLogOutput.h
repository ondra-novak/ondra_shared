


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

	virtual void writeToLog(const StrViewA &line, const std::time_t &, LogLevel ) {
		std::cerr << line << std::endl;
	}

	void sendToLog(const StrViewA &line, const std::time_t &time, LogLevel level) {
		std::lock_guard<std::mutex> _(lock);
		writeToLog(line, time, level);
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

	virtual bool start(LogLevel level, MutableStrViewA &buffer) override;
	virtual void sendBuffer(MutableStrViewA &text) override;
	virtual void commit(const MutableStrViewA &text) override;
	virtual PLogProvider newSection(const StrViewA &ident)  override;
	virtual void setProgress(float progressVal, int expectedCycles)  override;


	virtual bool isLogLevelEnabled(LogLevel level) const  override{
		return shared->isLogLevelEnabled(level);
	}
protected:
	std::vector<char> buffer;
	std::string ident;
	PFactory shared;
	time_t lastTime;
	LogLevel curLevel;


	void finishBuffer(const MutableStrViewA &b);
	void prepareBuffer(MutableStrViewA &b);
	static unsigned int getThreadIdent();

	virtual void appendDate(std::time_t now);
	virtual void appendLevel(LogLevel level);
	virtual void appendThreadIdent();
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

inline unsigned int StdLogProvider::getThreadIdent() {
	static std::atomic<unsigned int> threadCounter;
	static thread_local unsigned int curThreadId = ++threadCounter;
	return curThreadId;
}

inline void StdLogProvider::appendDate(std::time_t now) {

	auto wrfn = [&](char c) { buffer.push_back(c);};

	std::tm tinfo;
#ifdef _WIN32
	gmtime_s(&tinfo, &now);
#else
	gmtime_r(&now, &tinfo);
#endif

	unsignedToString(tinfo.tm_year+1900, wrfn, 10,4);
	buffer.push_back('-');
	unsignedToString(tinfo.tm_mon+1, wrfn, 10,2);
	buffer.push_back('-');
	unsignedToString(tinfo.tm_mday, wrfn, 10,2);
	buffer.push_back(' ');
	unsignedToString(tinfo.tm_hour, wrfn, 10,2);
	buffer.push_back(':');
	unsignedToString(tinfo.tm_min, wrfn, 10,2);
	buffer.push_back(':');
	unsignedToString(tinfo.tm_sec, wrfn, 10,2);
	buffer.push_back(' ');
}

inline void StdLogProvider::appendLevel(LogLevel level) {
	auto wrstr = [&](StrViewA c) { for (char x:c) buffer.push_back(x);};

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

}

inline void StdLogProvider::appendThreadIdent() {
	auto curThreadId = getThreadIdent();
	auto wrstr = [&](StrViewA c) { for (char x:c) buffer.push_back(x);};
	auto wrfn = [&](char c) { buffer.push_back(c);};

	buffer.push_back('[');
	unsignedToString(curThreadId, wrfn, 10,4);
	wrstr(ident);
	wrstr("] ");

}

inline bool StdLogProvider::start(LogLevel level, MutableStrViewA& b) {

	if (shared->isLogLevelEnabled(level)) {
		curLevel = level;
		buffer.clear();
		std::time(&lastTime);

		appendDate(lastTime);
		appendLevel(level);
		appendThreadIdent();
		b = MutableStrViewA(buffer.data(), buffer.size());
		prepareBuffer(b);
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
	shared->sendToLog(StrViewA(buffer.data(), buffer.size()), lastTime, curLevel);
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
