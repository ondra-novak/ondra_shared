/*
 * virtualMember.h
 *
 *  Created on: 10. 11. 2017
 *      Author: ondra
 */

#ifndef _ONDRA_SHARED_VIRTUALMEMBER_H_786987987
#define _ONDRA_SHARED_VIRTUALMEMBER_H_786987987
#include <stdexcept>

namespace ondra_shared {

///Virtual member is class which emulates member variable controlled by the outer class
/** You can inherit and then declare the VirtualMember variable anywhere in the
 * class inside of public section. However, virtual members should be
 * on most top of the class declaration otherwise it can run out of reach of the master.
 *
 *
 * VirtualMember occupies one byte (can be overriden if needed). In this
 * byte there is stored offset from the member to the address of its owner
 *
 * You need to initialize the vierual member by a pointer to the owner
 *
 *  VirtualMembers cannot be copied or assigned
 */

template<typename Master, typename OffsetType = unsigned char>
class VirtualMember {
public:
	VirtualMember(const Master *master):offset(calculateOffset(this,master)) {}
	VirtualMember(const VirtualMember &) = delete;
	void operator=(const VirtualMember &) = delete;



private:
	OffsetType offset;

	static const std::uintptr_t maxOffset = (1<<(sizeof(offset)*8))-1;


	static OffsetType calculateOffset(const VirtualMember *p, const Master *q) {
		std::uintptr_t ofs = reinterpret_cast<const char *>(p) - reinterpret_cast<const char *>(q);
		if (ofs>maxOffset) throw std::runtime_error("VirtualMember out of range");
		return (OffsetType)ofs;
	}
protected:

	const Master *getMaster() const {
		return reinterpret_cast<const Master *>(reinterpret_cast<const char *>(this)-offset);
	}
	 Master *getMaster() {
		return reinterpret_cast<Master *>(reinterpret_cast<char *>(this)-offset);
	}

};


}



#endif /* _ONDRA_SHARED_VIRTUALMEMBER_H_786987987 */
