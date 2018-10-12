/*
 * shared_function.cpp
 *
 *  Created on: 12. 10. 2018
 *      Author: ondra
 */

#include "../shared_function.h"
#include <fstream>
#include <iostream>

using namespace ondra_shared;

auto create_reader(const std::string &file) {
/*	class IOWrap: public std::ifstream {
	public:
		IOWrap(const std::string &s):std::ifstream(s),s(s) {}
		IOWrap(const IOWrap &other):std::ifstream(other.s),s(s) {}
	}*/

	std::ifstream input(file);
	return [input = std::move(input)]() mutable {
		return input.get();
	};
}


int main(int, char **) {
	shared_function<int()> reader(create_reader("shared_function.cpp"));

	int c = reader();
	while (c != EOF) {
		std::cout.put(static_cast<char>(c));
		c = reader();
	}



	return 0;
}
