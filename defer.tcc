/*
 * defer.tcc
 *
 *  Created on: 25. 5. 2018
 *      Author: ondra
 */

#ifndef SRC_ONDRA_SHARED_DEFER_TCC_5465465110015848
#define SRC_ONDRA_SHARED_DEFER_TCC_5465465110015848


namespace ondra_shared {
	thread_local IDeferContext *IDeferContext::include_defer_tcc_to_your_main_source = nullptr;
}


#endif /* SRC_ONDRA_SHARED_DEFER_TCC_5465465110015848 */
