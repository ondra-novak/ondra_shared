/*
 * filesystem.h
 *
 *  Created on: 5. 3. 2021
 *      Author: ondra
 */

#ifdef __clang__
#include <filesystem>
#else
#ifdef __GNUC__
#if __GNUC__ < 8
#include <experimental/filesystem>
namespace std {
	using namespace experimental;
}
#else
#include <filesystem>
#endif
#else
#include <filesystem>
#endif
#endif
