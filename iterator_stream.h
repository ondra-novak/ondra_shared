/*
 * iterator_stream.h
 *
 *  Created on: 3. 6. 2019
 *      Author: ondra
 */

#ifndef SRC_ONDRA_SHARED_ITERATOR_STREAM_H_78978408
#define SRC_ONDRA_SHARED_ITERATOR_STREAM_H_78978408


namespace ondra_shared {

template<typename Iter1, typename Iter2>
class IteratorStream {
public:
	template<typename T1, typename T2>
	IteratorStream(T2 &&cur, T1 &&end):cur(std::forward<T1>(cur)),end(std::forward<T2>(end)) {}

	bool operator!() const {
		return cur == end;
	}

	auto &&operator()() {
		auto &&ret = *cur;
		++cur;
		return ret;
	}

	auto operator *() const {
		return *cur;
	}

protected:
	Iter1 cur;
	Iter2 end;

};

///Creates stream from range of iterators
/** The stream is object, which supports three operations
 *
 *  !stream returns true, if there are no more items (!!stream return true, if there are more items)
 *  stream() returns next item
 *
 * @param begin begin of stream
 * @param end end of stream
 * @return
 */
template<typename T1, typename T2>
auto iterator_stream(T1 &&begin, T2 &&end) {
	return IteratorStream<std::remove_reference_t<T1>, std::remove_reference_t<T2> >(std::forward<T1>(begin),std::forward<T1>(end));
}

template<typename T>
auto iterator_stream(T &&container) {
	return IteratorStream<std::remove_reference_t<decltype(container.begin())>,
						  std::remove_reference_t<decltype(container.end())> >(container.begin(),container.end());
}


}

#endif /* SRC_ONDRA_SHARED_ITERATOR_STREAM_H_78978408 */
