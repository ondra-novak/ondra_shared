/*
 * cmdline.h
 *
 *  Created on: 18. 5. 2019
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_CMDLINE_H_20190238102938109
#define ONDRA_SHARED_CMDLINE_H_20190238102938109
#include <string>
#include <iostream>
#include <experimental/filesystem>
#include <optional>

namespace ondra_shared {


class CmdArgIter {
public:
	using PString = const char *;
	using PStringList = const PString *;

	CmdArgIter(PString arg0, unsigned int argc, PStringList argv)
		:arg0(arg0), argc(argc+1), arglist(argv-1) {
		initNextArg();
	}

	std::experimental::filesystem::path getProgramFullPath() const;


	///Returns true, if no more arguments reached.
	/**
	 *
	 * @retval true no more arguments
	 * @retval false more arguments are available
	 *
	 * If you need reversed operation, use double exclamation mark. !!
	 */
	bool operator!() const {
		return end;
	}

	///Retrieves next string argument.
	/**
	 * @return function returns next string argument. If the
	 * next argument is option, it is returned as string in its
	 * raw form. Function returns nullptr, if called after the end
	 */
	PString getNext();

	///Receives next option
	/**
	 * @return function returns next option (if there is any), or
	 * zero if not. If a option is returned, the function moves to next option
	 */
	char getOpt();

	///Receives next long option
	/**
	 * @return function returns next long option if there is any, or
	 * nullptr if not. If a option is returned, the function moves to the next option
	 */
	PString getLongOpt();

	///Recieves size of unprocessed arguments
	/** this can be used to pick unprocessed arguments and process them
	 * elswhere
	 * @return count of unprocessed arguments
	 */
	unsigned int size() const {return argc;}

	///Receives pointer to list of unprocessed arguments
	PStringList args() const {return arglist;}

	bool isText() const {
		return !end && !opt && (noopts || curarg[0] != '-');
	}


	bool isOpt() const {
		return opt || (!end && !isText() && curarg[1] != '-');
	}

	bool isEnd() const {
		return end;
	}

	bool isLongOpt() const {
		return !end && !isText() && curarg[1] == '-';
	}


	std::optional<std::uintptr_t> getUInt() ;

	std::optional<std::intptr_t> getInt() ;

	std::optional<double> getNumber() ;

protected:
	PString arg0;
	unsigned int argc;
	PStringList arglist;
	PString curarg = nullptr;
	bool end = false;
	bool opt = false;
	bool noopts= false;

	void initNextArg();

	static std::optional<std::uintptr_t> getUInt(PString a);
};


inline CmdArgIter parseCmdLine(int argc, char **argv) {
	return CmdArgIter(argv[0],argc-1, argv+1);
}

inline std::experimental::filesystem::path CmdArgIter::getProgramFullPath() const {
	using namespace std::experimental::filesystem;
	if (arg0[0] == '/') return arg0;
	auto p = current_path();
	return p/arg0;
}

inline CmdArgIter::PString CmdArgIter::getNext() {
	if (!end) {
		auto nc = curarg;
		opt  = false;
		initNextArg();
		return nc;
	} else {
		return nullptr;
	}
}

inline char CmdArgIter::getOpt() {
	if (opt) {
		if (std::isalnum(*curarg)) {
			char c = *curarg;
			initNextArg();
			return c;
		} else {
			return 0;
		}
	} else if (isOpt()) {
		opt=true;
		curarg++;
		return getOpt();
	}
	return 0;
}

inline CmdArgIter::PString CmdArgIter::getLongOpt() {
	if (isLongOpt()) {
		PString ret = curarg+2;
		initNextArg();
		return ret;
	} else {
		return nullptr;
	}
}


inline void CmdArgIter::initNextArg() {
	if (opt) {
		++curarg;
		if (*curarg == 0) {
			opt = false;
			initNextArg();
		}
	} else if (!end) {
		++arglist;
		--argc;
		curarg = *arglist;
		if (argc == 0) {
			end = true;
		} else if (*curarg == 0) {
			initNextArg();
		} else {
			if (isLongOpt()) {
				if (curarg[2] == 0) {
					noopts = true;
					initNextArg();
				}
			}
		}
	}
}

inline std::optional<std::uintptr_t> CmdArgIter::getUInt()  {
	PString k = getNext();
	return getUInt(k);
}

inline std::optional<std::uintptr_t> CmdArgIter::getUInt(PString k)  {
	std::optional<std::uintptr_t> ov;
	if (!k) return ov;

	uintptr_t v = 0;
	uintptr_t b = 10;
	while (*k) {
		char c = *k;
		if (c == 'x' && v == 0) b = 16;
		else if (c == 'b' && v == 0) b = 2;
		else if (c == 'o' && v == 0) b = 8;
		else {
			uintptr_t z;
			if (std::isdigit(c)) z = c - '0';
			else if (std::isalpha(c)) z = std::toupper(c) - 'A' + 10;
			else return ov;
			if (z >= b) return ov;
			v = v * b + z;
		}
		++k;
	}
	ov = v;
	return ov;
}

inline std::optional<std::intptr_t> CmdArgIter::getInt()  {
	PString k = getNext();
	std::optional<std::intptr_t> ov;
	if (!k) return ov;

	if (*k == '-') {
		auto v = getUInt(k+1);
		return -static_cast<intptr_t>(*v);
	} else {
		auto v = getUInt(k);
		return static_cast<intptr_t>(*v);
	}

}

inline std::optional<double>  CmdArgIter::getNumber()  {
	PString k = getNext();
	std::optional<double> ov;
	if (!k) return ov;

	char *z;
	double v = std::strtod(k,&z);
	if (*z == 0) ov = v;
	return ov;
}


}

#endif /* ONDRA_SHARED_CMDLINE_H_20190238102938109 */
