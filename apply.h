/*
 * apply.h
 *
 *  Created on: Apr 24, 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_SRC_SHARED_APPLY_H_464604698
#define ONDRA_SHARED_SRC_SHARED_APPLY_H_464604698

#include <utility>
#include <functional>

namespace ondra_shared {


template <typename Fnc, typename ... Types, std::size_t ... Indices>
auto apply_impl(Fnc &&fnc, const std::tuple<Types...> &tuple, std::index_sequence<Indices...>)
{
     return std::forward<Fnc>(fnc)(std::get<Indices>(tuple)...);
}

template <typename Fnc, typename ... Types>
auto apply(Fnc &&fnc, const std::tuple<Types...> &tuple)
{
     return apply_impl(std::forward<Fnc>(fnc), tuple, std::index_sequence_for<Types...>());
}



}


#endif /* ONDRA_SHARED_SRC_SHARED_APPLY_H_464604698 */

