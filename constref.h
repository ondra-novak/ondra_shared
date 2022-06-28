#ifndef __ONDRA_SHARED_CONSTREF_H_98231adg28e7hbdiw6_
#define __ONDRA_SHARED_CONSTREF_H_98231adg28e7hbdiw6_


namespace ondra_shared {

namespace _details {

template<typename T>
struct ConstRef {
public:
     typedef const T &Value;
};

template<typename T>
struct ConstRef<T &> {
public:
     typedef T &Value;
};

template<typename T>
struct ConstRef<const T &> {
public:
     typedef const T &Value;
};


}

template<typename T>
using const_ref = typename _details::ConstRef<T>::Value;


}

#endif
