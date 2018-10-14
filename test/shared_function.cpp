/*
 * shared_function.cpp
 *
 *  Created on: 12. 10. 2018
 *      Author: ondra
 */

#include "../shared_function.h"
#include <fstream>
#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>

using namespace ondra_shared;
using namespace std::chrono_literals;

auto create_reader(const std::string &file) {

	std::ifstream input(file);
	if (!input) throw std::runtime_error("file not found");

	return [input = std::move(input),mx=lambda_state<std::mutex>()]() mutable {
		std::this_thread::sleep_for(100ms);
		std::unique_lock<std::mutex> _(mx);
		return input.get();
	};
}


int main(int, char **) {

	shared_function<int()> reader(create_reader("shared_function.cpp"));

	auto readfn = [=] {
		int c = reader();
		while (c != EOF) {
			std::cout.put(static_cast<char>(c));
			c = reader();
		}
	};
	std::thread t1(readfn);
	std::thread t2(readfn);
	std::thread t3(readfn);
	std::thread t4(readfn);

	t1.join();
	t2.join();
	t3.join();
	t4.join();


	return 0;
}
