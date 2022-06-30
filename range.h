/*
 * range.h
 *
 *  Created on: 8. 6. 2019
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_SRC_SHARED_RANGE_H_14690567
#define ONDRA_SHARED_SRC_SHARED_RANGE_H_14690567
#include <utility>

///this declaration allows to use std::pair as range for range-for iteration.

template<typename T>
auto std::begin(const std::pair<T,T> &x) {return x.first;}
template<typename T>
auto std::end(const std::pair<T,T> &x) {return x.second;}



#endif /* ONDRA_SHARED_SRC_SHARED_RANGE_H_14690567 */
