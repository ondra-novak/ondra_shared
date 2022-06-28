#ifndef __ONDRA_SHARED_SHARED_RAII_5146848941361
#define __ONDRA_SHARED_SHARED_RAII_5146848941361

#include "raii.h"
#include "refcnt.h"

namespace ondra_shared {



template<typename Res, typename FnType, FnType fn>
class SharedRAIIImpl: public RAII<Res, FnType, fn>,  public RefCntObj {
public:

     using RAII<Res,FnType, fn>::RAII;
     SharedRAIIImpl() {}


};

template<typename Res, typename FnType, FnType fn>
using SharedRAII = RefCntPtr<SharedRAIIImpl<Res, FnType,Fn> >;

}
