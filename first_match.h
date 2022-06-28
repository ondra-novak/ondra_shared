/*
 * first_match.h
 *
 *  Created on: 11. 11. 2019
 *      Author: ondra
 */

#ifndef SRC_ONDRA_SHARED_FIRST_MATCH_H_2310912321ew1
#define SRC_ONDRA_SHARED_FIRST_MATCH_H_2310912321ew1
#include <algorithm>

namespace ondra_shared {

template<typename Pred, typename T>
auto first_match(Pred &&, T &&t) {
     return t;
}

template<typename Pred, typename T, typename ...Args>
auto first_match(Pred &&pred, T &&t1, T &&t2, Args&&... args) {
     if (pred(t1)) return t1;
     else return first_match(std::forward<Pred>(pred), std::forward<T>(t2), std::forward<Args>(args)...);
}


}





#endif /* SRC_ONDRA_SHARED_FIRST_MATCH_H_2310912321ew1 */
