/*
 * stack_alloc.h
 *
 *  Created on: 9. 9. 2022
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_SVO_VECTOR_H_ekd902du2232
#define ONDRA_SHARED_SVO_VECTOR_H_ekd902du2232

namespace ondra_shared {

template<typename T, std::size_t prealloc = 255/sizeof(T)+1>
class Vector {
public:

	using value_type = T;
	using iterator = T *;
	using const_iterator = const T *;

	Vector() = default;
	template<std::size_t p>
	Vector(const Vector<T,p> &other):Vector(other.begin(), other.end()) {}

	template<std::size_t p>
	Vector(Vector<T,p> &&other) {
		if (other.is_my_buffer(other._data) ) {
			for (T &n: other) {
				push_back(std::move(n));
			}
		} else {
			_data = other._data;
			_capacity = other._capacity;
			_length = other._length;
			other._data = reinterpret_cast<T *>(other._buffer);
			other._length = 0;
			other._capacity = prealloc;
		}
	}

	~Vector() {
		reset();
	}

	template<std::size_t p>
	Vector &operator=(const Vector<T, p> &other) {
		if (reinterpret_cast<const void *>(this) != reinterpret_cast<const void *>(&other)) {
			clear();
			append(other.begin(), other.end());
		}
		return *this;
	}

	template<std::size_t p>
	Vector &operator=(Vector<T, p> &&other) {
		if (reinterpret_cast<const void *>(this) != reinterpret_cast<const void *>(&other)) {
			reset();
			if (other.is_my_buffer(other._data)) {
				append_move(other.begin(), other.end());
			} else{
				_data = other._data;
				_capacity = other._capacity;
				_length = other._length;
				other._capacity = prealloc;
				other._size = 0;
				other._data = reinterpret_cast<T *>(other._buffer);
			}
		}
		return *this;
	}


	template<std::size_t p>
	void swap(Vector<T, p> &other) {
		if (is_my_buffer(_data) || other.is_my_buffer(other,_data)) {
			std::size_t common_cap = std::max(capacity(), other.capactity());
			reserve(common_cap);
			other.reserver(common_cap);
			std::size_t common_len = std::min(size(), other.size());
			for (std::size_t i = 0; i < common_len; i++) {
				std::swap(operator[](i),other[i]);
			}
			if (size() < other.size()) {
				for (std::size_t i = size(), end = other.size(); i<end; ++i) {
					push_back(std::move(other[i]));
				}
				other.resize(size());
			} else if (size() > other.size()) {
				for (std::size_t i = other.size(), end = size(); i<end; ++i) {
					other.push_back(std::move(operator[](i)));
				}
				resize(other.size());
			}
		} else {
			std::swap(_data, other._data);
			std::swap(_length, other._length);
			std::swap(_capacity, other,_capacity);
		}
	}

	T &operator[](std::size_t x){return _data[x];}
	const T &operator[](std::size_t x) const {return _data[x];}

	void resize(std::size_t newsz) {
		reserve(newsz);
		while (newsz > _length) push_back(T());
		while (newsz < _length) pop_back();
	}
	void resize(std::size_t newsz, const T &item) {
		reserve(newsz);
		while (newsz > _length) push_back(item);
		while (newsz < _length) pop_back();
	}

	template<typename Iter> Vector(Iter &&beg, Iter &&end) {
		append(std::forward<Iter>(beg), std::forward<Iter>(end));
	}

	void push_back(const T &x) {
		new(expand(1)) T(x);
	}
	void push_back(T &&x) {
		new(expand(1)) T(std::move(x));
	}

	void pop_back() {
		if (_length) shrink(1);
	}

	const T &front() const {
		return *_data;
	}
	const T &back() const {
		return _data[_length-1];
	}
	T &front() {
		return *_data;
	}
	T &back() {
		return _data[_length-1];
	}

	T *data() {return _data;}
	const T *data() const {return _data;}

	template<typename Iter, typename Transform>
	void append_transform(Iter &&beg, Iter &&end, Transform &&trn) {
		reserve_more(size()+std::distance(beg,end));
		for (Iter x = beg; x != end; ++x) {
			push_back(T(trn(*x)));
		}
	}

	template<typename Iter>
	void append(Iter &&beg, Iter &&end) {
		append_transform(std::forward<Iter>(beg), std::forward<Iter>(end)
				,[](const T &x)->const T &{return x;});
	}

	template<typename Iter>
	void append_move(Iter &&beg, Iter &&end) {
		append_transform(std::forward<Iter>(beg), std::forward<Iter>(end),&std::move<T>);
	}


	iterator begin() {return _data;}
	iterator end() {return _data+_length;}
	const_iterator begin() const {return _data;}
	const_iterator end() const {return _data+_length;}
	const_iterator cbegin() const {return _data;}
	const_iterator cend() const {return _data+_length;}

	void clear() {
		shrink(0);
	}
	void reset() {
		shrink(0);
		if (!is_my_buffer(_data)) {
			::operator delete(_data);
			_data = reinterpret_cast<T *>(_buffer);
			_length = 0;
			_capacity = prealloc;
		}
	}
	std::size_t size() const {
		return _length;
	}

	bool empty() const {
		return _length == 0;
	}

	///Reserver optimal capacity - can reserve more then requested
	/**
	 * @param newcap required capacity
	 * can reserve more capacity to avoid reallocation, when requested
	 * capacity delta is smaller than minimal expansion step;
	 */
	void reserve_more(std::size_t newcap) {
		reserve(std::max(_length*3/2, newcap));
	}

	void reserve(std::size_t newcap) {
		if (newcap<_capacity) return;
		void *n = ::operator new(sizeof(T)*newcap);
		T *nt = reinterpret_cast<T *>(n);
		std::for_each(_data, _data+_length, [&](T &x){
			new(nt) T(std::move(x));
			x.~T();
			++nt;
		});
		nt = reinterpret_cast<T *>(n);
		std::swap(nt, _data);
		if (!is_my_buffer(nt))
			::operator delete(nt);
		_capacity = newcap;
	}

	std::size_t capacity() const {
		return _capacity;
	}

	template<typename Iter, typename Transform>
	iterator insert_transform(const_iterator where, Iter &&beg, Iter &&end, Transform &&trn) {
		std::size_t len = std::distance(beg, end);
		if (len+_length > _capacity) {
			reserve_more(len+_length);
		}
		std::size_t pos = std::distance(_data, where);
		std::size_t x = _length;
		while (x > pos) {
			--x;
			new(_data+x+len) T(std::move(_data[x]));
			_data[x+len].~T();
		}
		for (auto iter = beg; iter != end; ++iter) {
			new(_data+pos) T(trn(*iter));
			++pos;
		}
		return _data+pos;
	}

	template<typename Iter>
	iterator insert(const_iterator where, Iter &&beg, Iter &&end) {
		insert_transform(where, std::forward<Iter>(beg), std::forward<Iter>(end),[](const T &x)->const T &{return x;});
	}
	template<typename Iter>
	iterator insert_move(const_iterator where, Iter &&beg, Iter &&end) {
		insert_transform(where, std::forward<Iter>(beg), std::forward<Iter>(end),&std::move<T>);
	}

	template<typename Iter>
	void erase(const_iterator from, const_iterator to) {
		auto beg = _data+std::distance(_data, from);
		auto end = _data+std::distance(_data, to);
		for (auto iter = beg; iter != end;++iter) iter->~T();
		auto fin = _data+_length;
		while (end != fin) {
			new(beg) T(std::move(*end));
			end->~T();
			++beg;
			++end;
		}
	}


protected:
	unsigned char _buffer[prealloc * sizeof(T)];

	T *_data = reinterpret_cast<T *>(_buffer);
	std::size_t _length = 0;
	std::size_t _capacity = prealloc;

	void *expand(std::size_t cnt) {
		if (cnt + _length > _capacity) {
			reserve_more(cnt+_length);
		}
		void *c = _data+_length;
		_length += cnt;
		return c;
	}

	void shrink(std::size_t newsz) {
		if (newsz > _length) return;
		while (_length > newsz) {
			_length--;
			_data[_length].~T();
		}
	}

	bool is_my_buffer(const T* nt) const
	{
		return reinterpret_cast<const void*>(nt) == reinterpret_cast<const void*>(_buffer);
	}

};



}




#endif /* ONDRA_SHARED_SVO_VECTOR_H_ekd902du2232 */
