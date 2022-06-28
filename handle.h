/*
 * handle.h
 *
 *  Created on: Jul 19, 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_HANDLE_H_547690451360
#define ONDRA_SHARED_HANDLE_H_547690451360

namespace ondra_shared {
///Handle object - replacement for RAII
/**
 * Works the same, but allow to declare invalid value
 *
 * @tparam T Handle type
 * @tparam CloseFn Type of function to close handle
 * @tparam pointer Pointer to function to close handle
 * @tparam invval Pointer to value cointaing the invalid value. Must be valid pointer
 */
template<typename T, typename CloseFn, CloseFn closeFn, const T *invval>
class Handle {
public:
     Handle():h(*invval) {}
     Handle(T &&h):h(std::move(h)) {}
     Handle(const T &h):h(h) {}
     Handle(Handle &&other):h(other.h) {other.h = *invval;}
     Handle &operator=(Handle &&other) {
          if (this != &other) {
               close();
               h = other.h;
               other.h = *invval;
          }
          return *this;
     }
     operator T() const {return h;}
     T get() const {return h;}
     T operator->() const {return h;}
     void close() {
          if (!is_invalid())  closeFn(h);
          h = *invval;
     }
     ~Handle() {close();}
     T detach() {T res = h; h = *invval; return res;}
     bool is_invalid() const {return h == *invval;}
     bool operator !() const {return is_invalid();}

     ///Allow to init as pointer to value
     /**Function also closes current handle and returns pointer to value */
     T *init() {
          close();
          return &h;
     }
     const T *ptr() const {return &h;}
protected:
     T h;
};

template<typename T>
struct Handle_PointerTraits {
     static T *null;
     static void freefn(T *val) {free(static_cast<void *>(val));}
     typedef void (*FreeFn)(T *);
};

template<typename T> T *Handle_PointerTraits<T>::null = nullptr;

///C vanilla pointer, with many predefined values
template<typename T,
           typename CloseFn = typename Handle_PointerTraits<T>::FreeFn,
           CloseFn closeFn = &Handle_PointerTraits<T>::freefn,
            T  *const*invval = &Handle_PointerTraits<T>::null>
using CPtr = Handle<T *, CloseFn, closeFn, invval>;


}



#endif /* ONDRA_SHARED_HANDLE_H_547690451360 */
