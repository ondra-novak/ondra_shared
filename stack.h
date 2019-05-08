#pragma once

#ifndef _ONDRA_SHARED_48970987948_STACK_H_
#define _ONDRA_SHARED_48970987948_STACK_H_
#include "refcnt.h"


namespace ondra_shared {

	namespace __details {

		template<typename T>
		class StackItem: public RefCntObj {
		public:
			StackItem(const T &v, const RefCntPtr<StackItem> &parent):v(v),parent(parent) {}
			StackItem(T &&v, const RefCntPtr<StackItem> &parent):v(std::move(v)),parent(parent) {}

			const T &getValue() const {return v;}
			T &getValue() {return v;}
			void swap(T &other) {std::swap(other,v);}
			const RefCntPtr<StackItem> &getParent() const {return parent;}
		protected:
			T v;
			RefCntPtr<StackItem> parent;
		};


	}

	///Declaration of immutable MT-safe shared stack
	/**
	 * Every stack operation creates a new head. Other thread can hold old stack's
	 * head and it is not affected by this operation.
	 */
	template<typename T>
	class SharedStack {
	public:
		SharedStack() {}

		SharedStack push(const T &v);
		SharedStack push(T &&v);
		const T &top() const;
		T &top();
		SharedStack pop();
		bool empty();

	protected:
		SharedStack(RefCntPtr<__details::StackItem<T> > head):head(head) {}
		RefCntPtr<__details::StackItem<T> > head;
	};

template<typename T>
inline SharedStack<T> SharedStack<T>::push(const T& v) {
	return SharedStack(new __details::StackItem<T>(v,head));
}

template<typename T>
inline SharedStack<T> SharedStack<T>::push(T&& v) {
	return SharedStack(new __details::StackItem<T>(std::move(v),head));
}

template<typename T>
inline const T& SharedStack<T>::top() const {
	return head->getValue();
}

template<typename T>
inline T& SharedStack<T>::top() {
	return head->getValue();
}


template<typename T>
inline SharedStack<T> SharedStack<T>::pop() {
	return SharedStack(head->getParent());
}

template<typename T>
inline bool SharedStack<T>::empty() {
	return head == nullptr;
}

}

#endif
