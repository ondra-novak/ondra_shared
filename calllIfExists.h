/*
 * calllIfExists.h
 *
 *  Created on: 14. 11. 2017
 *      Author: ondra
 */

#ifndef __ONDRA_SHARED_CALLLIFEXISTS_H_23472938472
#define __ONDRA_SHARED_CALLLIFEXISTS_H_23472938472

namespace ondra_shared {


namespace _details {

struct can_call_test
{
    template<typename F, typename... A>
    static decltype(std::declval<F>()(std::declval<A>()...), std::true_type())
    f(int);

    template<typename F, typename... A>
    static std::false_type
    f(...);
};


template<typename Fn, typename... A>
struct call_if_can {
	template <typename.... Args>



};

}

///calls function if exists. Can also detect whether the function exists
/**
 * @tparam Fn callable
 */
template<typename Fn>
class CallIfExists {


	template<typename Args...>
	auto calll(Args&&... args) -> auto {



	}


};



}



#endif /* __ONDRA_SHARED_CALLLIFEXISTS_H_23472938472 */
