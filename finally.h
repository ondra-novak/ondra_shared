#ifndef _ONDRA_SHARED_FINALLY_234832901
#define _ONDRA_SHARED_FINALLY_234832901

namespace ondra_shared {

template<typename Fn>
class FinallyImpl {
public:
	FinallyImpl(const Fn &fn):fn(fn),e(true) {}
	FinallyImpl(Fn &&fn):fn(std::move(fn)),e(true) {}
	FinallyImpl(const FinallyImpl &) = delete;
	FinallyImpl(FinallyImpl &&o):fn(std::move(o.fn)),e(o.e) {o.e = false;}
	FinallyImpl &operator=(FinallyImpl &&o) {
		if (&o != this) {
			fn = std::move(o.fn);
			e = o.e;
			o.e = false;
		}
		return *this;
	}

	~FinallyImpl() noexcept(false) {
		if (e) fn();
	}
protected:
	Fn fn;
	bool e = false;
};

template<typename Fn>
FinallyImpl<Fn> finally(Fn &&fn) {return FinallyImpl<Fn>(std::forward<Fn>(fn));}


}


#endif
