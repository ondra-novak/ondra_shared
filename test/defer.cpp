/*
 * defer.cpp
 *
 *  Created on: 25. 5. 2018
 *      Author: ondra
 */



#include <iostream>
#include <fstream>
#include "../defer.h"
#include "../defer.tcc"



using namespace ondra_shared;


void golang_defer1 () {
	DeferStack defer;

	defer >> []{
			std::cout << "world" << std::endl;
	};

	std::cout << "hello" << std::endl;
}

void golang_defer2 () {
	DeferStack defer;

	std::cout << "counting" << std::endl;

	for (auto i = 0; i < 10; i++) {
		defer >> [=]{
			std::cout << i << std::endl;
		};
	}

}

void count_recursive(int remain) {
	if (remain == 0) {
		std::cout << "stop"  << std::endl;
	} else {
		std::cout << "before call (" << remain <<")" << std::endl;

		if (remain > 5) {
			count_recursive(remain-1);
		} else {
			defer >> [=]{count_recursive(remain-1);};
		}

		std::cout << "after call (" << remain <<")" << std::endl;
	}
}

void global_defer_demo () {
	DeferContext ctx(defer_root);

	count_recursive(10);

}


int main(int argc, char **argv) {

	std::cout << "====golang defer1====" << std::endl;
	golang_defer1();
	std::cout << "====golang defer2 (counting)====" << std::endl;
	golang_defer2();
	std::cout << "====global defer demo====" << std::endl;
	global_defer_demo();
	return 0;

}
