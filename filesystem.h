/*
 * filesystem.h
 *
 *  Created on: 5. 3. 2021
 *      Author: ondra
 */

#ifndef SRC_SHARED_FILESYSTEM_H_
#define SRC_SHARED_FILESYSTEM_H_

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


#endif /* SRC_SHARED_FILESYSTEM_H_ */
