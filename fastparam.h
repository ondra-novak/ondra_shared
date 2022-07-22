/*
 * fastparam.h
 *
 *  Created on: 20. 7. 2022
 *      Author: ondra
 */

#ifndef SRC_LIBS_SHARED_FASTPARAM_H_1208sjqoiwdj823dsjqoid21
#define SRC_LIBS_SHARED_FASTPARAM_H_1208sjqoiwdj823dsjqoid21

namespace ondra_shared {

///Converts Type to its const Type reference variant if it is possible.
template<typename Type> struct FastParamT {using T = const Type &;};
template<typename Type> struct FastParamT<Type &> {using T = Type &;};
template<typename Type> struct FastParamT<Type &&> {using T = Type &&;};
template<typename Type> struct FastParamT<Type *> {using T = Type *;};
template<typename Type> struct FastParamT<const Type *> {using T = const Type *;};
template<typename T> using FastParam = typename FastParamT<T>::T;



}



#endif /* SRC_LIBS_SHARED_FASTPARAM_H_1208sjqoiwdj823dsjqoid21 */
