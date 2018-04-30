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


template<typename ... Args>
void foo(Args && ... args) {
	argList(0, std::forward<Args>(args)...);
}

#define WRAP(f) \
    [&] (auto&&... args) -> decltype(auto) \
    { return f (std::forward<decltype(args)>(args)...); }

int main(int argc, char **argv) {

	auto tuple = std::make_tuple("Hello", "world", 42, 12.3, true);
	apply(wrap_template_fn(foo), tuple);

}


