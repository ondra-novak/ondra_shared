/*
 * linux_spawn.h
 *
 *  Created on: 28. 12. 2019
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_SRC_SHARED_LINUX_SPAWN_H_0123897198s891
#define ONDRA_SHARED_SRC_SHARED_LINUX_SPAWN_H_0123897198s891
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <sstream>

namespace ondra_shared {


///Class allows to spawn external process and access its standard input and output
/**
 * To spawn new process, use ExternalProcess::spawn().
 */
class ExternalProcess {
public:

	///Wraps standard file descriptor
	class FD {
	public:
		int fd;
		FD(int fd):fd(fd) {}
		FD():fd(-1) {}
		~FD() {close();}
		FD(FD &&other):fd(other.fd) {other.fd = -1;}
		FD &operator=(FD &&other) {if (fd != other.fd) {close(); fd = other.fd; other.fd = -1;} return *this;}
		operator int() const {return fd;}
		void close() {if (fd >= 0) ::close(fd); fd = -1;}
		int detach() {int z = fd; fd = -1;return z;}
	};

	///Wraps pipe
	struct Pipe {
		FD read,write;
	};

	ExternalProcess(pid_t pid, FD &&stdin, FD &&stdout, FD &&stderr)
		:stdin(std::move(stdin))
		,stdout(std::move(stdout))
		,stderr(std::move(stderr))
		,pid(pid) {}

	FD stdin;
	FD stdout;
	FD stderr;
	pid_t pid;
	int status = 0;

	auto reader() {return [in = std::move(stdout)]() mutable -> int {
		unsigned char c;
		auto r = ::read(in.fd,&c,1);
		if (r == 0) return -1;
		else return c;
	};};

	auto writer() {return [out = std::move(stdin)](int x) mutable {
		unsigned char c = static_cast<unsigned char>(x);
		::write(out.fd, &c, 1);
	};};

	auto error() {return [in = std::move(stderr)]() mutable -> int {
		unsigned char c;
		auto r = ::read(in.fd,&c,1);
		if (r == 0) return -1;
		else return c;
	};};

	ExternalProcess(ExternalProcess &&other)
		:stdin(std::move(other.stdin))
		,stdout(std::move(other.stdout))
		,stderr(std::move(other.stderr))
		,pid(other.pid) {other.pid = 0;}

	~ExternalProcess() {
		join();
	}

	///Waits to process exit
	int join() {
		if (pid) {
			waitpid(pid,&status,0);
			pid = 0;
		}
		return status;
	}


	enum Status {
		running,
		normal_exit,
		signal_exit
	};

	struct ExitStatus {
		Status st;
		int code;
	};

	ExitStatus getExitStatus() const {
		if (pid) return {running};
		if (WIFSIGNALED(status)) {
			return {signal_exit, WTERMSIG(status)};
		} else {
			return {normal_exit, WEXITSTATUS(status)};
		}
	}

	bool isRunning()  {
		if (pid) {
			if (waitpid(pid,&status,WNOHANG) == 0) return true;
			pid = 0;
		}
		return false;
	}

	bool sendSignal(int signal) {
		return !::kill(pid, signal);
	}

	bool kill() {
		return sendSignal(SIGTERM);
	}

	///Detaches process (returns its pid)
	pid_t detach() {
		pid_t x = pid;
		pid = 0;
		return x;
	}

	class Exception: public std::runtime_error {
	public:
		static std::string buildError(int e, std::string desc) {
			std::ostringstream buff;
			buff << "System exception: " << e << " " << std::strerror(e) << " while '" << desc << '"';
			return buff.str();
		}

		Exception(int e, const char *desc):std::runtime_error(buildError(e,desc)),errnr(e),desc(desc) {}
		Exception(int e, const std::string &desc):std::runtime_error(buildError(e,desc)),errnr(e),desc(desc) {}

		int errnr;
		std::string desc;

	};

	static Pipe makePipe() {
		int tmp[2];
		int r = pipe2(tmp, O_CLOEXEC);
		if (r < 0) throw Exception(errno, "pipe2");
		return {FD(tmp[0]), FD(tmp[1])};
	}

	///Spawns command line
	/** Command line contains process name (with path) followed by arguments separated by white spaces
	 * If a white space must be part of argument, the whole argument must be enclosed to quoted. You
	 * can also use backslash to escape quotes or white spaces
	 *
	 *
	 *
	 * @param workDir
	 * @param cmd_line
	 * @return
	 */
	static ExternalProcess spawn_cmdline(std::string workDir, std::string cmd_line) {
		std::vector<std::string> args;
		std::string buff;
		bool qt = false;
		bool sl = false;
		bool wqt = false;
		for (char c: cmd_line) {
			if (sl) {
				switch(c) {
				case 'n': buff.push_back('\n');break;
				case 'r': buff.push_back('\r');break;
				case 't': buff.push_back('\t');break;
				case 'a': buff.push_back('\a');break;
				case 'b': buff.push_back('\b');break;
				case '0': buff.push_back('\0');break;
				default: buff.push_back(c);
				}
				sl = false;
			}
			else if (isspace(c)) {
				if (qt) buff.push_back(c);
				else if (!buff.empty() || wqt) {
					args.push_back(buff);
					buff.clear();
					wqt =false;
				}
			} else if (c == '"') {
				qt = !qt;
				wqt = true;
			} else {
				buff.push_back(c);
			}
		}
		if (!buff.empty() || wqt) {
			args.push_back(buff);
		}
		if (args.empty()) throw std::runtime_error("ExternalProcess: - given empty command line");
		std::string pgmname ( std::move(args[0]));
		args.erase(args.begin());
		return spawn(workDir, pgmname, args);
	}


	static ExternalProcess spawn(std::string workDir,
			std::string execPath, std::vector<std::string> params) {

		Pipe proc_input (makePipe());
		Pipe proc_output (makePipe());
		Pipe proc_error (makePipe());
		Pipe proc_control (makePipe());
		size_t arglen = params.size()+2;

		char ** arglist = (char **)alloca(sizeof(char *)*arglen);
		size_t z = 0;
		arglist[z++] = const_cast<char *>(execPath.c_str());
		for (auto &&c: params) {
			arglist[z++] = const_cast<char *>(c.c_str());
		}
		arglist[z] = nullptr;
		pid_t frk = fork();
		if (frk < 0) {
			throw Exception(errno, "Fork failed");
		} else if (frk == 0) {
			try {
				if (chdir(workDir.c_str())) throw Exception(errno, "chdir");
				if (dup2(proc_input.read, 0)<0) throw Exception(errno, "dup->stdin");
				if (dup2(proc_output.write, 1)<0) throw Exception(errno, "dup->stdout");
				if (dup2(proc_error.write, 2)<0) throw Exception(errno, "dup->stderr");
				execvp(arglist[0], arglist);
				throw Exception(errno, "execlp");
			} catch (Exception &e) {
				int r1 = write(proc_control.write,&e.errnr, sizeof(e.errnr));
				int r2 = write(proc_control.write,e.desc.c_str(), e.desc.length());
				(void)r1;
				(void)r2;

			} catch (std::exception &e) {
				const char *w = e.what();
				int er = 0;
				int r1 = write(proc_control.write,&er, sizeof(er));
				int r2 = write(proc_control.write, w, strlen(w));
				(void)r1;
				(void)r2;
			}
			exit(0);
			throw;
		} else {
			proc_control.write.close();
			int error;
			auto r = read(proc_control.read, &error, sizeof(error));
			if (r == sizeof(error)) {
				std::string errmsg;
				char buff[256];
				auto r = read(proc_control.read, buff, sizeof(buff));
				while (r > 0) {
					errmsg.append(buff,r);
					r = read(proc_control.read, buff, sizeof(buff));
				}
				throw Exception(error, errmsg);
			}
			return ExternalProcess(frk,
					std::move(proc_input.write),
					std::move(proc_output.read),
					std::move(proc_error.read));
		}
	}
};
}





#endif /* ONDRA_SHARED_SRC_SHARED_LINUX_SPAWN_H_0123897198s891 */
