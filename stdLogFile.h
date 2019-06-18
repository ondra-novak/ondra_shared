


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
	 */
	StdLogFile(StrViewA pathname,
			LogLevel minLevel = LogLevel::info)
		:StdLogProviderFactory( minLevel)
		,pathname(pathname)
		,outfile(this->pathname,std::ios::app)
		,autorotate_count(0)
		{
		AbstractLogProvider::rotated(autorotate_count);
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

	virtual void writeToLog(const StrViewA &line, const std::time_t &, LogLevel) override {
		if (AbstractLogProvider::rotated(autorotate_count)) {
			outfile << "Log rotated..." <<std::endl;
			reopenLog();
			outfile << "Continues..." <<std::endl;

		}
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
	int autorotate_count;



};




}


#endif /* _ONDRA_SHARED_STDLOGFILE_H_23312319080809 */
