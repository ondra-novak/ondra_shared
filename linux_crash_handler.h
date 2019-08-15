/*
 * linux_crash_handler.h
 *
 *  Created on: 30. 7. 2019
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_SRC_SHARED_LINUX_CRASH_HANDLER_H_woiweuowidwiuoqe23047289
#define ONDRA_SHARED_SRC_SHARED_LINUX_CRASH_HANDLER_H_woiweuowidwiuoqe23047289
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cxxabi.h>
#include <execinfo.h>
#include <csignal>

#include "linear_map.h"


namespace ondra_shared {


class SignalMap: public linear_map<int, void *> {
public:

	static SignalMap &getInstance() {
		static SignalMap map;
		return map;
	}
};

class CrashHandler {
	std::function<void(const char *)> print_line;
public:
	template<typename Fn>
	explicit CrashHandler(Fn &&print_line_fn):print_line(std::move(print_line_fn)) {}

	void backtrace(int sig) {
		std::string sbuff;
		sbuff.resize(256);
		std::sprintf(sbuff.data(), "Crashed on signal: %d", sig); print_line(sbuff.data());
		void *pointers[300];
		std::size_t ptrsz;
		char **strings;

		ptrsz = ::backtrace (pointers, 300);
		strings = backtrace_symbols (pointers, ptrsz);





		char *mbuff = 0;
		std::size_t mlen = 0;

		for (std::size_t i = 0; i < ptrsz; i++) {
			char *line = strings[i];
			char *beg_mangled = strrchr(line,'(');
			if (beg_mangled) {
				beg_mangled++;
				char *end_mangled = strrchr(beg_mangled, '+');
				if (end_mangled) {
					*end_mangled = 0;
					int status;
					char *res = abi::__cxa_demangle(beg_mangled, mbuff, &mlen, &status);
					*end_mangled = '+';
					if (res) {
						mbuff = res;
						sbuff.clear();
						sbuff.append(line, beg_mangled-line);
						sbuff.append(mbuff);
						sbuff.append(end_mangled);
						print_line(sbuff.c_str());
						continue;
					}
				}
			}
			print_line(line);
		}

		free(mbuff);
		std::free (strings);
	}

	static void handler(int sig) {
		SignalMap &map = SignalMap::getInstance();
		auto iter = map.find(sig);
		if (iter != map.end()) {
			CrashHandler *inst = reinterpret_cast<CrashHandler *>(iter->second);
			inst->backtrace(sig);
		}
		signal(sig,SIG_DFL);
		raise(sig);
	}

	void install(int sig) {
		SignalMap &map = SignalMap::getInstance();
		map[sig] = this;
		signal(sig, &handler);
	}

	///Install to all crashing signals
	void install() {
		install(SIGSEGV);
		install(SIGBUS);
		install(SIGILL);
		install(SIGABRT);
	}

};




}





#endif /* ONDRA_SHARED_SRC_SHARED_LINUX_CRASH_HANDLER_H_woiweuowidwiuoqe23047289] */
