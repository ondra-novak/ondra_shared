/*
 * scheduler.cpp
 *
 *  Created on: 30. 4. 2018
 *      Author: ondra
 */



#include <fstream>
#include "../scheduler.h"
#include "../future.h"
#include <chrono>

using namespace std::literals::chrono_literals;


using namespace ondra_shared;

int main(int argc, char **argv) {


	auto sch = Scheduler::create();

	auto repid = sch.each(0.3s) >> []{
			std::cout << "called repeated action"<< std::endl;
	};

	sch.after(1s) >> []{
			std::cout << "called after 1 second"<< std::endl;
	};

	sch.after(2s) >> []{
			std::cout << "called after 2 second"<< std::endl;
	} >> [=]{
		std::cout << "removed repeated action"<< std::endl;
		sch.remove(repid);
	};
	sch.after(3s) >> []{
			std::cout << "called after 3 second"<< std::endl;
	};


	WaitableEvent ev;
	sch.after(4s) >> []{
			std::cout << "called after 4 second"<< std::endl;
	} >> ([&ev]{
		ev.signal();
	});
	ev.wait();
}

