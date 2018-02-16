/*
 * vla.h
 *
 *  Created on: 16. 2. 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_SRC_SHARED_VLA_H_
#define ONDRA_SHARED_SRC_SHARED_VLA_H_

#include <cstddef>
#include "stringview.h"


namespace ondra_shared {


///VLA object is Variable Length Array for pure C++ without using any hack, such a alloca or malloca
/**
 * Objects VLA are mostly allocated on stack. You need to specify count of items reserved on the stack. If
 * there is request to allocate more items, the allocation is performed on heap
 *
 * @tparam Type type of the item in the array. It can be common C++ object (it is not limited to PODs)
 * @tparam reserved count of items reserved at stack. You need to specify arbitrary number designed
 * for the specific situation in the code.
 *
 *
 * @note VLA objects cannot be copied or moved
 *
 * VLA objects can be iterated using for-ranged-loop
 */
template<typename Type, std::size_t reserved>
class VLA: public MutableStringView<Type> {
public:

	///Construct variable length array
	/**
	 *
	 * @param count count of requested items
	 */
	template<typename ... Args>
	VLA(std::size_t count, Args&& ... args)
		:MutableStringView<Type>(alloc(this,count,std::forward<Args>(args) ...),count) {}

	VLA(const StringView<Type> data):VLA(data.length) {
		auto iter = this->begin();
		for (auto &&item: data) {
			*iter = item;
			++iter;
		}
	}


	///copying is disabled
	VLA(const VLA &other) = delete;
	///moving is disabled
	VLA(VLA &&other) = delete;
	VLA &operator=(const VLA &other) = delete;
	VLA &operator=(VLA &&other) = delete;

	~VLA() {
		dealloc(this->data, this->length);
	}


protected:
	unsigned char buffer[reserved*sizeof(Type)];

	template<typename ... Args>
	static Type *alloc(VLA *me, std::size_t count,  Args&& ... args) {
		Type * items;
		if (count > reserved) {
			items = reinterpret_cast<Type *>(operator new(sizeof(Type) * count));
		} else {
			items = reinterpret_cast<Type *>(me->buffer);
		}
		for (auto i = 0; i < count; i++) {
			try {
				new(items+i) Type(std::forward<Args>(args) ...);
			} catch (...) {
				while (i > 0) {
					--i;
					items[i].~Type();
				}
				if (count > reserved) operator delete(items);
				throw;
			}
		}
		return items;
	}
	static void dealloc(Type *items, std::size_t count) {
		for (auto i = count; i > 0;) {
			--i;
			items[i].~Type();
		}
		if (count > reserved) operator delete(items);
	}

};


}



#endif /* SRC_SHARED_VLA_H_ */
