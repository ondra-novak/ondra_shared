/*
 * future_test.cpp
 *
 *  Created on: 24. 4. 2018
 *      Author: ondra
 */

#include <thread>
#include <iostream>
#include <sstream>
#include "../future.h"
#include "../apply.h"

using namespace ondra_shared;

FutureValue<int> testVal1;
FutureValue<int> testVal2;
FutureValue<int> testVal3;


auto testChain(Future<int> &f) {
	return (

	!( //try
		f >> [](int x) {
			//print test
			std::cout << "Value:" << x << std::endl;
			return x;
		} >> [](int x) {
			//convert
			std::ostringstream sbuff;
			sbuff << x;
			return sbuff.str();
		} >> [](std::string a) {
			//print test
			std::cout << "Value as string: " << a << std::endl;
			return a;
		} >> [](std::string a) {
			Future<std::string> z;
			std::thread thr([z,a]() mutable {
				std::this_thread::sleep_for(std::chrono::seconds(1));
				z.set("Delayed "+a);
			});
			thr.detach();
			return z;  //return std::string
		} >> [](std::string a) {
			//print test
			std::cout << "Value as string (2): " << a << std::endl;
			return a;
		}

	) >> [](const std::exception_ptr &e) { //catch (return std::string)
		try {
			std::rethrow_exception(e);
		} catch (std::exception &e) {
			return std::string("Exception: ")+e.what();
		}
	});
}


template<typename Fn, typename... Args>
void callInLambda(Fn &&fn, Args && ... args) {
	auto z = std::make_tuple(std::forward<Args>(args)...);
	std::remove_reference_t<Fn> xfn(fn);
	auto zfn = [z,xfn]{
		apply(xfn,z);
	};
	zfn();
}


int main(int argc, char **argv) {

	callInLambda([](int a, int b, const char *c, double d){
		std::cout << a << b << c << d << std::endl;
	},10,20,"ahoj",32.23);

	testVal1.await([](int v) {
		std::cout << "Handle1: " << v << std::endl;
	});

	testVal1.set(42);


	std::thread thr1([]{
			std::this_thread::sleep_for(std::chrono::seconds(3));
			testVal2.set(56);
	}) ;

	int v = testVal2.get();
	std::cout << "Handle2: " << v << std::endl;
	thr1.join();


	std::thread thr2([]{
			std::this_thread::sleep_for(std::chrono::seconds(3));
			testVal3.set(78);
	}) ;

	auto waitable = testVal3.create_waitable_event();
	while (!waitable->wait_for(std::chrono::seconds(1))) {
		std::cout << "wait timeout, restarting" << std::endl;
	}

	v = testVal3.get();
	std::cout << "Handle3: " << v << std::endl;
	thr2.join();

	Future<int> f1;
	auto f2 = testChain(f1);
	f1.set(42);
	std::cout << "Future1:" << f2.get() << std::endl;

	Future<int> f3;
	auto f4 = testChain(f3);
	f3.reject(std::make_exception_ptr(std::runtime_error("Rejected by exception")));
	std::cout << "Future1:" << f4.get() << std::endl;

}
