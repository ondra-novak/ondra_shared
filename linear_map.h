/*
 * linear_map.h
 *
 *  Created on: 8. 5. 2019
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_LINEAR_MAP_H_24867654568
#define ONDRA_SHARED_LINEAR_MAP_H_24867654568
#include <stdexcept>
#include "linear_set.h"

namespace ondra_shared {

//TODO Not tested
template<typename Key, typename Value, typename Less = std::less<Key> >
class linear_map {

	template<typename K>
	class SrchKey: public std::pair<const K &, bool> {
	public:
		SrchKey(const K &k):std::pair<const K &, bool>(k,false) {}
	};

public:
	using value_type = std::pair<Key, Value>;
	struct Compare {
		Less less;
		Compare(const Less &less):less(less) {}
		template<typename A, typename B, typename C, typename D>
		bool operator()(const std::pair<A,B> &a, const std::pair<C,D> &b) const {
			return less(a.first, b.first);
		}
	};
	using Set = linear_set<value_type, Compare>;

	using iterator = typename Set::iterator;
	using const_iterator = typename Set::const_iterator;
	using reverse_iterator = typename Set::reverse_iterator;
	using const_reverse_iterator = typename Set::const_reverse_iterator;
	using key_type = Key;
	using size_type = typename Set::size_type;
	using key_compare = Less;
	using value_compare = Compare;

	void clear() {
		return dset.clear();
	}

	std::pair<iterator,bool> insert( const value_type& value ) {
		return dset.insert(value);
	}
	std::pair<iterator,bool> insert( value_type&& value ) {
		return dset.insert(std::move(value));
	}
	template< class InputIt >
	void insert( InputIt first, InputIt last ) {
		return dset.insert(first,last);
	}
	void insert( std::initializer_list<value_type> ilist ) {
		return dset.insert(ilist);
	}

	template< class... Args >
	std::pair<iterator,bool> emplace( Args&&... args ) {
		return dset.emplace(std::forward<Args>(args)...);
	}
	iterator erase( iterator pos ) {
		return dset.erase(pos);
	}
	iterator erase( const_iterator pos ) {
		return dset.erase(pos);
	}
	iterator erase( const_iterator first, const_iterator last ) {
		return dset.erase(first,last);
	}
	size_type erase( const key_type& key ) {
		return dset.erase(std::pair<const key_type &, bool>(key,false));
	}
	void swap( linear_map& other ) noexcept {
		dset.swap(other.dset);
	}
	void swap( std::vector<value_type>& other ) noexcept {
		dset.swap(other);
	}

	bool empty() const noexcept {return dset.empty();}
	size_type size() const noexcept {return dset.size();}
	size_type max_size() const noexcept {return dset.max_size();}

	template< class K > size_type count( const K& key ) const {
		return dset.count(SrchKey<K>(key));
	}
	template< class K > iterator find( const K& key ) {
		return dset.find(SrchKey<K>(key));
	}
	template< class K > const_iterator find( const K& key ) const {
		return dset.find(SrchKey<K>(key));
	}
	template< class K > bool contains( const K& key ) const {
		return dset.contains(SrchKey<K>(key));
	}
	template< class K > std::pair<iterator,iterator> equal_range( const K& key ) {
		return dset.equal_range(SrchKey<K>(key));
	}
	template< class K > std::pair<const_iterator,const_iterator> equal_range( const K& key ) const {
		return dset.equal_range(SrchKey<K>(key));
	}
	template< class K > iterator lower_bound(const K& key) {
		return dset.lower_bound(SrchKey<K>(key));
	}
	template< class K > const_iterator lower_bound(const K& key) const {
		return dset.lower_bound(SrchKey<K>(key));
	}
	template< class K > iterator upper_bound(const K& key) {
		return dset.upper_bound(SrchKey<K>(key));
	}
	template< class K > const_iterator upper_bound(const K& key) const {
		return dset.upper_bound(SrchKey<K>(key));
	}

	key_compare key_comp() const {return dset.value_comp().less;}
	value_compare value_comp() const {return dset.value_comp();}

	linear_map():dset(Compare((Less()))) {}
	explicit linear_map( const Less& comp ):dset(Compare(comp)) {}
	template< class InputIt > linear_map( InputIt first, InputIt last, const Less& comp = Less())
		:dset(first,last,Compare(comp)) {}
	linear_map( const linear_map& other )
		:dset(other.dset) {}
	linear_map(linear_map&& other )
		:dset(std::move(other.dset)) {}
	linear_map( std::initializer_list<value_type> init, const Less& comp = Less())
		:dset(init,Compare(comp)) {}
	explicit linear_map(std::vector<value_type>&& other, const Less& comp = Less() )
			:dset(std::move(other), Compare(comp)) {}

	linear_map &operator=(const linear_map & other) {
		dset = other.dset;
		return *this;
	}
	linear_map &operator=(linear_map && other) {
		dset = std::move(other.dset);
		return *this;
	}

	iterator begin() {return dset.begin();}
	iterator end() {return dset.end();}
	const_iterator begin() const {return dset.cbegin();}
	const_iterator end() const {return dset.cend();}
	const_iterator cbegin() const {return dset.cbegin();}
	const_iterator cend() const {return dset.cend();}
	reverse_iterator rbegin() {return dset.rbegin();}
	reverse_iterator rend() {return dset.rend();}
	const_reverse_iterator crbegin() const {return dset.crbegin();}
	const_reverse_iterator crend() const {return dset.crend();}

	template< class K >
	const Value &at(const K &key) const {
		auto f = find(key);
		if (f == cend()) throw std::out_of_range("Key not found");
		else return (*f).second;
	}

	template< class K >
	Value &operator[](const K &key) {
		auto z = insert(value_type(key, Value()));
		return z.first->second;
	}

	void reserve(std::size_t sz) {
		return dset.reserve(sz);
	}
	std::size_t capacity() const {
		return dset.capacity();
	}

protected:
	Set dset;

};

}





#endif /* ONDRA_SHARED_LINEAR_MAP_H_24867654568 */
