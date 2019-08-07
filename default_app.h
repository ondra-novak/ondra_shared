#include <cctype>
#include <cstring>
#include <iostream>
#include <optional>
#include "cmdline.h"
#include "ini_config.h"
#include "linear_map.h"
#include "stdLogOutput.h"

namespace ondra_shared {





class DefaultApp {
public:

	bool debug = false;
	bool verbose = false;
	IniConfig config;
	PStdLogProviderFactory logProvider;
	std::optional<CmdArgIter> args;
	std::experimental::filesystem::path appPath;
	std::experimental::filesystem::path configPath;
	const char *config_default_name = nullptr;
	const char *help_banner = nullptr;
	const char *log_section = "log";
	std::ostream &output;

	struct Switch {
		const char short_sw;
		const char *long_sw;
		std::function<void(CmdArgIter &)> handler;
		const char *help;
	};

	DefaultApp(const std::initializer_list<Switch> &switches,
				std::ostream &output,
				const char *help_banner = nullptr)
	:help_banner(help_banner)
	,output(output)
	,switches(switches)
	 {}


	bool init(int argc, char **argv) {
		return init(CmdArgIter(argv[0],argc-1,argv+1));
	}

	bool init(CmdArgIter &&cmdIter) {

		const std::initializer_list<Switch> *ptr_default_switches;
		const std::initializer_list<Switch> & default_switches = {
				Switch{'h',"help",[&](CmdArgIter&){this->showHelp(*ptr_default_switches);exit(0);},"show this help"},
				Switch{'d',"debug",[&](CmdArgIter&){debug=true;},"enable debug"},
				Switch{'v',"verbose",[&](CmdArgIter&){verbose=true;},"verbose mode"},
				Switch{'f',"config",[&](CmdArgIter&args){
					configPath=args.getNext();
					if (configPath.is_relative())
						configPath = std::experimental::filesystem::current_path() / configPath;
				},"<config_file> specify path to configuration file"}
		};
		ptr_default_switches = &default_switches;

		bool res = process_switches(cmdIter, default_switches);

		this->args = std::move(cmdIter);

		if (!res) return false;

		config.load(configPath);
		auto logcfg = config[log_section];
		logProvider = StdLogFile::create(
						verbose?std::string(""):logcfg["file"].getPath(""),
						debug?StrViewA(""):logcfg["level"].getString(""),
						LogLevel::debug);
		logProvider->setDefault();
		return true;

	}

	bool process_switches(CmdArgIter &cmdIter, const std::initializer_list<Switch> &default_switches) {

		appPath = cmdIter.getProgramFullPath();
		configPath = appPath;
		configPath.remove_filename();
		configPath /= "..";
		configPath /= "conf";
		configPath /= (config_default_name?config_default_name:appPath.filename().string().append(".conf").c_str());



		while (!!cmdIter) {
			std::initializer_list<Switch>::const_iterator iter;
			char short_sw = 0;
			const char *long_sw = nullptr;
			if (cmdIter.isOpt()) short_sw = cmdIter.getOpt();
			else if (cmdIter.isLongOpt()) long_sw = cmdIter.getLongOpt();
			else break;

			auto pred = [&](const Switch &s) {
				if (long_sw) return std::strcmp(s.long_sw , long_sw) == 0;
				else return  s.short_sw == short_sw;
			};

			iter = std::find_if(switches.begin(), switches.end(), pred);
			if (iter == switches.end()) {
				iter = std::find_if(default_switches.begin(), default_switches.end(), pred);
				if (iter == default_switches.end()) {
					return false;
				}
			}

			iter->handler(cmdIter);
		}


		return true;

	}


	virtual void showHelp(const std::initializer_list<Switch> &defsw) {
		if (help_banner) output << help_banner << std::endl << std::endl;

		int spaceReq = 8;
		for (auto &&sw:switches) {
			std::size_t len = std::strlen(sw.long_sw);
			spaceReq = std::max<int>(len, spaceReq);
		}

		int spacePrefix = spaceReq + 9;

		auto print = [&](const Switch &sw) {
			if (sw.short_sw) output << "-" << sw.short_sw << " ";
			else output << "   ";

			if (sw.long_sw) output << "--" << sw.long_sw << " ";
			else output << "   ";

			output << " ";

			for (int i = spaceReq-(sw.long_sw?std::strlen(sw.long_sw):0); i > 0; --i) {
				output.put(' ');
			}

			wordwrap(sw.help, spacePrefix);
		};

		for (auto &&sw:switches) {
			print(sw);
		}
		for (auto &&sw: defsw) {
			print(sw);
		}
	}

	void wordwrap(const char *text, int swspace = 0) {
		int linelen = 65-swspace;
		if (linelen < 0) linelen = 0;
		int l = linelen;
		while (*text) {
			if (*text == '\n' || (l <= 0 && std::isspace(*text))) {
				output << std::endl;
				for (int i = 0; i < swspace; i++) {
					output.put(' ');
				}
				l = linelen;
			} else {
				output.put(*text);
				l--;
			}
			text++;
		}
		output << std::endl;
	}

	virtual ~DefaultApp() {}

protected:

	const std::initializer_list<Switch> &switches;

};


}
