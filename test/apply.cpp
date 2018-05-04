/*
 * apply.cpp
 *
 *  Created on: 30. 4. 2018
 *      Author: ondra
 */

#include <iostream>
#include <tuple>
#include "../apply.h"



using namespace ondra_shared;


void argList(int ) {}

template<typename T, typename ... Args>
void argList(int count,T && t,  Args && ... args) {
	std::cout << "Argument " << count << ": " << std::forward<T>(t) << std::endl;
	argList(count+1,std::forward<Args>(args)...);
}


struct Foo {
	template<typename ... Args>
	void operator()(Args && ... args) {
		argList(0, std::forward<Args>(args)...);
	}
};


int main(int argc, char **argv) {

	auto tuple = std::make_tuple("Hello", "world", 42, 12.3, true);
	apply(Foo(), tuple);

}


