/*
 * range.h
 *
 *  Created on: 8. 6. 2019
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_SRC_SHARED_RANGE_H_14690567
#define ONDRA_SHARED_SRC_SHARED_RANGE_H_14690567
#include <algorithm>


namespace ondra_shared {

template<typename T1, typename T2>
class Range {
public:
	Range(T1 &&beg, T2 &&end)
		:itr_beg(std::forward<T1>(beg))
		,itr_end(std::forward<T2>(end)) {}


	const T1 &begin() const {return itr_beg;}
	const T2 &end() const {return itr_end;}

protected:
	T1 itr_beg;
	T2 itr_end;
};


template<typename T1, typename T2>
Range<T1, T2> range(T1 &&beg, T2 &&end) {
	return Range<T1, T2>(std::forward<T1>(beg), std::forward<T2>(end));
}

}


#endif /* ONDRA_SHARED_SRC_SHARED_RANGE_H_14690567 */
