/*
 * linear_map.h
 *
 *  Created on: 8. 5. 2019
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_LINEAR_SET_H_87986548794654
#define ONDRA_SHARED_LINEAR_SET_H_87986548794654

#include <algorithm>
#include <functional>
#include <vector>
#include <utility>

namespace ondra_shared {

//TODO Not tested

/**
 * Implements std::set but all items are sorted in a vector. The
 * search is done using binary_search algoritm.
 *
 * The object is slow for inserting new items, because items
 * must be inserted to the correct place, which involves moving the other items
 * in the vector. However, it is slightly faster to insert multiple items
 * because sorting is performed only once per batch. When batch of items
 * is inserted, the batch is internally sorted, and then merged with current
 * set.
 *
 * Iterating through the items is fast like iterating through the vector.
 *
 */
template<typename T, typename Less = std::less<T> >
class linear_set {


public:
	using VecT = std::vector<T>;

	using iterator = typename VecT::iterator;
	using const_iterator = typename VecT::const_iterator;
	using reverse_iterator = typename VecT::reverse_iterator;
	using const_reverse_iterator = typename VecT::const_reverse_iterator;
	using value_type = T;
	using key_type = T;
	using size_type = typename VecT::size_type;
	using key_compare = Less;
	using value_compare = Less;

	void clear();

	std::pair<iterator,bool> insert( const value_type& value );
	std::pair<iterator,bool> insert( value_type&& value );
	template< class InputIt >
	void insert( InputIt first, InputIt last );
	void insert( std::initializer_list<value_type> ilist );
	template< class... Args >
	std::pair<iterator,bool> emplace( Args&&... args );
	iterator erase( iterator pos );
	iterator erase( const_iterator pos );
	iterator erase( const_iterator first, const_iterator last );
	template<typename K>
	size_type erase( const K& key );
	void swap( linear_set& other ) noexcept;
	void swap( std::vector<value_type>&  other ) noexcept;

	bool empty() const noexcept {return data.empty();}
	size_type size() const noexcept {return data.size();}
	size_type max_size() const noexcept {return data.max_size();}

	template< class K > size_type count( const K& key ) const;
	template< class K > iterator find( const K& x );
	template< class K > const_iterator find( const K& x ) const;
	template< class K > bool contains( const K& x ) const;
	template< class K > std::pair<iterator,iterator> equal_range( const K& x );
	template< class K > std::pair<const_iterator,const_iterator> equal_range( const K& x ) const;
	template< class K > iterator lower_bound(const K& x);
	template< class K > const_iterator lower_bound(const K& x) const;
	template< class K > iterator upper_bound(const K& x);
	template< class K > const_iterator upper_bound(const K& x) const;

	key_compare key_comp() const;
	value_compare value_comp() const;

	linear_set();
	explicit linear_set( const Less& comp );
	template< class InputIt > linear_set( InputIt first, InputIt last, const Less& comp = Less());

	linear_set( const linear_set& other );
	linear_set(linear_set&& other );
	linear_set( std::initializer_list<value_type> init, const Less& comp = Less());
	explicit linear_set(std::vector<T>&& other , const Less& comp = Less());

	linear_set &operator=(const linear_set & other) {
		data = other.data;
		less = other.less;
		return *this;
	}
	linear_set &operator=(linear_set && other) {
		data = std::move(other.data);
		less = std::move(other.less);
		return *this;
	}


	iterator begin();
	iterator end();
	const_iterator cbegin() const;
	const_iterator cend() const;
	reverse_iterator rbegin();
	reverse_iterator rend();
	const_reverse_iterator crbegin() const;
	const_reverse_iterator crend() const;

	void reserve(std::size_t sz);
	std::size_t capacity() const;

protected:

	VecT data;
	Less less;

	std::pair<iterator,bool> find_exists(const value_type &v) ;
};


template<typename T, typename Less>
void linear_set<T, Less>::clear() {
	data.clear();
}

template<typename T, typename Less>
typename linear_set<T, Less>::key_compare linear_set<T, Less>::key_comp() const {
	return less;
}

template<typename T, typename Less>
typename linear_set<T, Less>::value_compare linear_set<T, Less>::value_comp() const {
	return less;
}

template<typename T, typename Less>
void linear_set<T, Less>::reserve(std::size_t sz) {
	data.reserve(sz);
}

template<typename T, typename Less>
std::size_t linear_set<T, Less>::capacity() const {
	return data.capacity();
}

template<typename T, typename Less>
std::pair<typename linear_set<T, Less>::iterator, bool>
	linear_set<T, Less>::find_exists(const value_type& value) {
	auto where = lower_bound(value);
	if (where == end()) return {where, true};
	return {where, less(value, *where)};
}

template<typename T, typename Less>
std::pair<typename linear_set<T, Less>::iterator, bool>
	linear_set<T, Less>::insert( const value_type& value ) {

	auto f = find_exists(value);
	if (!f.second) return f;
	data.insert(f.first, value);
	return f;
}

template<typename T, typename Less>
std::pair<typename linear_set<T, Less>::iterator, bool>
	linear_set<T, Less>::insert( value_type&& value ) {

	auto f = find_exists(value);
	if (!f.second) return f;
	auto dist = f.first - data.begin();
	data.insert(f.first, std::move(value));
	return std::make_pair(data.begin()+dist,true);
}

template<typename T, typename Less>
template< class InputIt >
void linear_set<T, Less>::insert( InputIt first, InputIt last ) {
	VecT tmp(first,last);
	if (tmp.size() < 5) {
		for (auto &&x : tmp) {
			insert(std::move(x));
		}
	} else {
		std::sort<typename VecT::iterator, Less &>(tmp.begin(), tmp.end(), less);
		VecT newvect;

		auto l = data.begin();
		auto r = tmp.begin();
		auto le = data.end();
		auto re = tmp.end();
		while (l != le && r != re) {
			if (less(*l, *r)) {
				newvect.push_back(std::move(*l));
				++l;
			} else if (less(*r, *l)) {
				newvect.push_back(std::move(*r));
				++r;
			} else {
				newvect.push_back(std::move(*r));
				++r;
				++l;
			}
		}
		while (l != le) {
			newvect.push_back(std::move(*l));
			++l;
		}
		while (r != re) {
			newvect.push_back(std::move(*r));
			++r;
		}
		std::swap(data, newvect);
	}

}

template<typename T, typename Less>
void linear_set<T, Less>::insert( std::initializer_list<value_type> ilist ) {
	insert(ilist.begin(), ilist.end());
}

template<typename T, typename Less>
template< class... Args >
std::pair<typename linear_set<T, Less>::iterator,bool> linear_set<T, Less>::emplace( Args&&... args ) {
	return insert(T(std::forward<Args>(args)...));
}


template<typename T, typename Less>
typename linear_set<T, Less>::iterator linear_set<T, Less>::erase(iterator pos) {
	return data.erase(pos);
}

template<typename T, typename Less>
typename linear_set<T, Less>::iterator linear_set<T, Less>::erase(const_iterator pos) {
	return data.erase(pos);
}

template<typename T, typename Less>
typename linear_set<T, Less>::iterator linear_set<T, Less>::erase(const_iterator first,
		const_iterator last) {
	return data.erase(first,last);
}

template<typename T, typename Less>
template<typename K>
typename linear_set<T, Less>::size_type linear_set<T, Less>::erase(const K& key) {
	auto f = lower_bound(key);
	if (f != end() && !less(key, *f)) {
		erase(f);
		return 1;
	} else {
		return 0;
	}
}

template<typename T, typename Less>
void linear_set<T, Less>::swap(linear_set& other) noexcept {
	std::swap(data,other.data);
	std::swap(less,other.less);
}

template<typename T, typename Less>
template<class K>
typename linear_set<T, Less>::size_type linear_set<T, Less>::count(const K& key) const {
	auto f = lower_bound(key);
	return f != cend() && !less(key, *f);
}

template<typename T, typename Less>
template< class K >
typename linear_set<T, Less>::iterator linear_set<T, Less>::find( const K& x ) {
	auto f = lower_bound(x);
	auto e = end();
	if (f != e && !less(x, *f)) return f;
	return e;
}
template<typename T, typename Less>
template< class K >
typename linear_set<T, Less>::const_iterator linear_set<T, Less>::find( const K& x ) const {
	auto f = lower_bound(x);
	auto e = cend();
	if (f != e && !less(x, *f)) return f;
	return e;
}

template<typename T, typename Less>
template< class K >
bool linear_set<T, Less>::contains( const K& x ) const {
	auto f = lower_bound(x);
	auto e = cend();
	return (f != e && !less(x, *f));
}

template<typename T, typename Less>
template< class K >
std::pair<typename linear_set<T, Less>::iterator,typename linear_set<T, Less>::iterator> linear_set<T, Less>::equal_range( const K& x ) {
	return std::equal_range<iterator, const K &, const Less &>(data.begin(), data.end(), x, less);
}
template<typename T, typename Less>
template< class K >
std::pair<typename linear_set<T, Less>::const_iterator,typename linear_set<T, Less>::const_iterator> linear_set<T, Less>::equal_range( const K& x ) const {
	return std::equal_range<const_iterator, const K &, const Less &>(data.begin(), data.end(), x, less);
}
template<typename T, typename Less>
template< class K >
typename linear_set<T, Less>::iterator linear_set<T, Less>::lower_bound(const K& x) {
	return std::lower_bound<iterator, const K &, const Less &>(data.begin(), data.end(), x, less);
}
template<typename T, typename Less>
template< class K >
typename linear_set<T, Less>::const_iterator linear_set<T, Less>::lower_bound(const K& x) const {
	return std::lower_bound<const_iterator, const K &, const Less &>(data.begin(), data.end(), x, less);
}
template<typename T, typename Less>
template< class K >
typename linear_set<T, Less>::iterator linear_set<T, Less>::upper_bound(const K& x) {
	return std::upper_bound<iterator, const K &, const Less &>(data.begin(), data.end(), x, less);
}
template<typename T, typename Less>
template< class K >
typename linear_set<T, Less>::const_iterator linear_set<T, Less>::upper_bound(const K& x) const {
	return std::upper_bound<const_iterator, const K &, const Less &>(data.begin(), data.end(), x, less);
}

template<typename T, typename Less>
typename linear_set<T, Less>::iterator linear_set<T, Less>::begin() {
	return data.begin();
}
template<typename T, typename Less>
typename linear_set<T, Less>::iterator linear_set<T, Less>::end() {
	return data.end();
}
template<typename T, typename Less>
typename linear_set<T, Less>::const_iterator linear_set<T, Less>::cbegin() const {
	return data.cbegin();
}
template<typename T, typename Less>
typename linear_set<T, Less>::const_iterator linear_set<T, Less>::cend() const {
	return data.cend();
}
template<typename T, typename Less>
typename linear_set<T, Less>::reverse_iterator linear_set<T, Less>::rbegin(){
	return data.rbegin();
}
template<typename T, typename Less>
typename linear_set<T, Less>::reverse_iterator linear_set<T, Less>::rend() {
	return data.rend();
}
template<typename T, typename Less>
typename linear_set<T, Less>::const_reverse_iterator linear_set<T, Less>::crbegin() const {
	return data.crbegin();
}
template<typename T, typename Less>
typename linear_set<T, Less>::const_reverse_iterator linear_set<T, Less>::crend() const {
	return data.crend();

}

template<typename T, typename Less>
linear_set<T, Less>::linear_set() {}

template<typename T, typename Less>
linear_set<T, Less>::linear_set( const Less& comp ):less(comp) {}

template<typename T, typename Less>
template< class InputIt >
linear_set<T, Less>::linear_set( InputIt first, InputIt last, const Less& comp)
:data(first,last),less(comp) {
	std::sort<iterator, Less &>(data.begin(),data.end(),less);
}

template<typename T, typename Less>
linear_set<T, Less>::linear_set( const linear_set& other )
:data(other.data),less(other.less) {}

template<typename T, typename Less>
linear_set<T, Less>::linear_set(linear_set&& other ):less(std::move(other.less)) {
	swap(other);
}

template<typename T, typename Less>
linear_set<T, Less>::linear_set( std::initializer_list<value_type> init, const Less& comp)
	:linear_set(init.begin(), init.end(), comp)
{}

template<typename T, typename Less>
linear_set<T, Less>::linear_set(std::vector<T>&& other, const Less& comp)
:data(std::move(other)), less(comp) {
	std::sort<iterator,const Less &>(data.begin(),data.end(),comp);
}

template<typename T, typename Less>
void linear_set<T, Less>::swap( std::vector<value_type>&  other ) noexcept {
	std::swap(other, data);
	if (!data.empty())
		std::sort<iterator,const Less &>(data.begin(),data.end(),less);
}


}

#endif /* ONDRA_SHARED_LINEAR_SET_H_87986548794654 */
