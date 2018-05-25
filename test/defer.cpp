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

int main(int argc, char **argv) {

	///create defer root
	defer >> [] {

			for (int i = 0; i < 100; i++) {
				if (i & 1) std::cout << "normal:" << i << std::endl;
				else defer >> [=]{std::cout << "defered:"<<i << std::endl;};
			}

	};
	return 0;

}
