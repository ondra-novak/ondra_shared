/*
 * param_pack.h
 *
 *  Created on: 12. 10. 2018
 *      Author: ondra
 */

#ifndef ONDRA_SHARED_TEST_PARAM_PACK_H_2323dweqweqd
#define ONDRA_SHARED_TEST_PARAM_PACK_H_2323dweqweqd

namespace ondra_shared {



template<typename ... Args> struct param_pack;

namespace _details {
template<typename H, typename ... T> struct param_pack_pop { using type = param_pack<T ...>;};
template<typename H, typename ... T> struct param_pack_top { using type = H;};
};

///Allows to store param_pack as single type
/**
 * Parameter pack are multiple types packed under single literal. They are not considered as type. This
 * make manipulation with the packs harder.
 *
 * The template class param_pack makes this issue much easier. You can put a parameter pack
 * into this template which is also a type. The type can be used as ususal.
 * However, it also brings some limitations.
 *
 * Once you put pack into the template class, it is very complicated to pull it out. You cannot
 * use param_pack instead of arguments of a function to instantiate a function prototype. If you seek
 * for the way, explore function "instatiate"
 *
 * The template param_pak keeps parameter pack, however its type striktly depends on the parameters
 * and on top of it, it has variable arguments.
 *
 * You can use "functions" to manage parameters in the pack. Each of this function is
 * actually subtype with template arguments. In many cases, you need to pretend the keyword "typename"
 * or "template" (especially if it is template)
 *
 *
 *
 *
 */
template<typename ... Args>
class param_pack {
public:

	///Inserts a new type into param_pack. Result is new param_pack
	/**
	 * @code
	 * using foo = param_pack<int, int, bool>;
	 * using bar = foo::push<std::string>;
	 *    //result is param_pack<std::string, int, int, bool>
	 * @endcode
	 */
    template<typename T> using push = param_pack<T, Args ...>;

	///Appends a new type into param_pack. Result is new param_pack
	/**
	 * @code
	 * using foo = param_pack<int, int, bool>;
	 * using bar = foo::append<std::string>;
	 *    //result is param_pack<int, int, bool, std::string>
	 * @endcode
	 */

    template<typename T> using append = param_pack<Args ..., T>;

	///Removes first item from the pack
	/**
	 * @code
	 * using foo = param_pack<std::string, int, int, bool>;
	 * using bar = foo::pop;
	 *    //result param_pack<int, int, bool>;
	 * @endcode
	 */
    using pop = typename _details::param_pack_pop<Args ...>::type;

    ///Returns the first item from the pack
	/**
	 * @code
	 * using foo = param_pack<std::string, int, int, bool>;
	 * using bar = foo::top
	 *    //result is std::string
	 * @endcode
	 */
    using top = typename _details::param_pack_top<Args ...>::type;

    ///Converts each type in the parameter pack
	/**
	 * @code
	 * using foo = param_pack<std::string &, int &, int, bool &&>;
	 * using bar = foo::template convert<std::remove_reference>
	 *    //result is param_pack<std::string , int , int, bool >;
	 * @endcode
	 */
    template<template<typename> class convert_fn> using convert = typename pop::template convert<convert_fn>::template push<typename convert_fn<top>::type >;

    ///Uses parameter pack to instantiate an other class
    /** This generates instance of new class from a template by using all parameters as arguments of
     * its template. You can optianally specify additional types which are used before the param_pack
     *
     * @code
     * using foo = param_pack<std::string, int, int, bool>;
     * using my_tuple = foo:instantiate<std::tuple, float>
     *   //result std::tuple<float, std::string, int, int, bool>
     * @endcode
     */

    template<template<typename ...> class c, typename ... K> using instantiate = c<K ..., Args ...>;

};

///Specialization for empty
template<>
struct param_pack<> {

    template<typename T> using push = param_pack<T>;
    template<typename T> using append = param_pack<T>;
    using pop = param_pack<>;
    template<template<typename> class convert_fn> using convert = param_pack<>;
    template<template<typename ...> class c, typename ... K> using instantiate = c<K ...>;


};



}


#endif /* ONDRA_SHARED_TEST_PARAM_PACK_H_2323dweqweqd */
