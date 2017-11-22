


#ifndef _ONDRA_SHARED_STDLOGFILE_H_23312319080809
#define _ONDRA_SHARED_STDLOGFILE_H_23312319080809

#include "stdLogOutput.h"
#include <fstream>

namespace ondra_shared {


class StdLogFile: public StdLogProviderFactory<StdStreamLogOutputFn> {
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
		:StdLogProviderFactory<StdStreamLogOutputFn>(outfile, minLevel)
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
		AbstractLogProvider::getInstance() = create();
	}

	void reopenLog() {
		outfile.close();
		outfile.open(pathname, std::ios::app);
	}


protected:

	std::string pathname;
	std::ofstream outfile;
	unsigned int autorotate_count;
	std::string autorotate_compress;



};




}


#endif /* _ONDRA_SHARED_STDLOGFILE_H_23312319080809 */
