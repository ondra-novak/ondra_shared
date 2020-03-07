


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
		outfile.flush();
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

class StdLogFileRotating: public StdLogProviderFactory  {
public:

	template<typename Str>
	StdLogFileRotating(Str &&pathname, LogLevel minLevel = LogLevel::info, unsigned int rotate_count=7, unsigned int rotate_interval = 86400)
		:StdLogProviderFactory ( minLevel)
		 ,pathname(std::forward<Str>(pathname))
		 ,rotate_count(rotate_count)
		 ,rotate_interval(rotate_interval)
		 {

		day_num = readLastDayNumber();
		if (day_num) {
			outfile.open(pathname, std::ios::app);
		}
	}


	void doRotate() {
		std::string n1;
		std::string n2;
		appendNumber(n2, rotate_count);
		for (int i = rotate_count; i > 1; i--) {
			std::swap(n2,n1);
			appendNumber(n2,i-1);
			std::rename(n2.c_str(),n1.c_str());
		}
		std::rename(pathname.c_str(),n2.c_str());

	}

	void appendNumber(std::string &buff, int number) {
		buff.clear();
		buff.append(pathname);
		buff.push_back('.');
		unsignedToString(number, [&](char c){buff.push_back(c);}, 10, 4);
	}

	unsigned int readLastDayNumber() {
		std::ifstream f(pathname, std::ios::in);
		if (!f) return 0;
		std::string buff;
		std::getline(f,buff);
		auto p = buff.rfind(':');
		if (p == buff.npos) return 0;
		const char *nr = buff.c_str()+p+1;
		return std::strtoul(nr,nullptr, 10);
	}

	virtual void writeToLog(const StrViewA &line, const std::time_t &t, LogLevel) override {
		unsigned int d = t/rotate_interval;
		if (d != day_num) {
			outfile.close();
			day_num = readLastDayNumber();
			if (day_num != d) doRotate();
			outfile.open(pathname,std::ios::app);
			outfile << "Rotation serial nr.: " << d << std::endl;
		}
		outfile << line << std::endl;
		outfile.flush();
	}


	static PStdLogProviderFactory create(StrViewA pathname, LogLevel minLevel, unsigned int rotate_count = 7, unsigned int rotate_interval = 86400) {
		if (pathname.empty()) return new StdLogProviderFactory(minLevel);
		else return new StdLogFileRotating(pathname, minLevel, rotate_count, rotate_interval);
	}

	static PStdLogProviderFactory create(StrViewA pathname, StrViewA level, LogLevel defaultLevel, unsigned int rotate_count = 7, unsigned int rotate_interval = 86400) {
		LogLevelToStrTable lstr;
		auto l = lstr.fromString(level,defaultLevel);
		return create(pathname, l, rotate_count, rotate_interval);
	}

protected:
	std::string pathname;
	std::ofstream outfile;
	unsigned int rotate_count;
	unsigned int day_num;
	unsigned int rotate_interval;
};





}


#endif /* _ONDRA_SHARED_STDLOGFILE_H_23312319080809 */
