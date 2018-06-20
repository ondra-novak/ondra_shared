


#ifndef _ONDRA_SHARED_STDLOGFILE_H_23312319080809
#define _ONDRA_SHARED_STDLOGFILE_H_23312319080809

#include "stdLogOutput.h"
#include <fstream>

namespace ondra_shared {


class StdLogFile: public StdLogProviderFactory {
public:

	///The class takes care about writting to log file
	/**
	 *
	 * @param pathname the pathname to the log file. It is hightly recommended
	 * to specify full pathname (relative pathaname is not advised).
	 * @param minLevel sets minimum allowed log level (default info)
	 * @param autorotate_count specifies count of rotates before the log is erased.
	 *
	 * @param autorotate_compress
	 */
	StdLogFile(StrViewA pathname,
			LogLevel minLevel = LogLevel::info,
			unsigned int autorotate_count = 0,
			StrViewA autorotate_compress = StrViewA())
		:StdLogProviderFactory( minLevel)
		,pathname(pathname)
		,outfile(this->pathname,std::ios::app)
		,autorotate_count(autorotate_count)
		,autorotate_compress(autorotate_compress){
	}

	bool operator! () const {
		return !outfile;
	}

	operator bool() const {
		return !(!outfile);
	}

	void setCurrent() {
		AbstractLogProviderFactory::getInstance() = this;
		AbstractLogProvider::getInstance() = StdLogProviderFactory::create();
	}

	void reopenLog() {
		outfile.close();
		outfile.open(pathname, std::ios::app);
	}

	virtual void writeToLog(const StrViewA &line, const std::time_t &) override {
		outfile << line << std::endl;
	}


	///Create standard log to file
	/**
	 * @param pathname pathname to file, if empty, stderr is used
	 * @param minLevel minimal level
	 * @return log provider
	 */
	static PStdLogProviderFactory create(StrViewA pathname, LogLevel minLevel) {
		if (pathname.empty()) return new StdLogProviderFactory(minLevel);
		else return new StdLogFile(pathname, minLevel);
	}

	///Create standard log to file
	/**
	 * @param pathname pathname to file, if empty, stderr is used
	 * @param level text value for level
	 * @param defaultLevel value used if the text value is not resolved or empty
	 * @return log provider
	 */
	static PStdLogProviderFactory create(StrViewA pathname, StrViewA level, LogLevel defaultLevel) {
		LogLevelToStrTable lstr;
		auto l = lstr.fromString(level,defaultLevel);
		return create(pathname, l);
	}

protected:

	std::string pathname;
	std::ofstream outfile;
	unsigned int autorotate_count;
	std::string autorotate_compress;
	std::time_t nextLogRotateTime;



};




}


#endif /* _ONDRA_SHARED_STDLOGFILE_H_23312319080809 */
