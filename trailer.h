/*
 * trailer.h
 *
 *  Created on: 27. 5. 2022
 *      Author: ondra
 */

#ifndef SRC_ONDRA_SHARED_TRAILER_H_6765144087064
#define SRC_ONDRA_SHARED_TRAILER_H_6765144087064
#include <memory>

namespace ondra_shared {

///Constructs a trailer object
/**
 * Trailer object allows to scheduler a function call to the end of the current scope.
 *
 * To use this function correctly, declare a variable by using 'auto' declarator
 *
 * @code
 * auto t = trailer([]{ ... code executed at end of the scope ...});
 * @endcode
 *
 * The instance itself allows to add further trailes or cancel the trailer as needed
 *
 * @param fn function/lambda to be scheduled <void()>
 * @return Trailer object
 *
 * @note Trailer object is allocated on stack. It cannot be copied or moved
 */
template<typename Fn> auto trailer(Fn &&fn);

///Trailer object
/**
 * Trailer object can be constructed empty. To reduce allocations, you can reserve
 * some bytes on stack where trailer functions are placed. If this space is exhausted,
 * further trailers are allocated on the stack
 *
 * @param buffsz space reserved for trailers. Default value allows to store trailer consists
 * from 14x pointer-sized variables + 1x vtable + 1x pointer to the function
 *
 */
template<std::size_t buffsz=sizeof(void *)*16> class Trailer;



class Trailer_Defs {
protected:
    class AbstractCall {
    public:
        virtual ~AbstractCall() {}
        virtual void run() noexcept = 0;
        virtual AbstractCall *move(char *buff, std::size_t &pos, std::size_t size) = 0;
    };


    using Ptr = std::unique_ptr<AbstractCall>;

    template<typename Fn>
    class SingleCall:public AbstractCall {
    public:
        SingleCall(Fn &&fn):fn(std::forward<Fn>(fn)) {}
        virtual void run() noexcept {fn();}
        AbstractCall *move(char *buff, std::size_t &pos, std::size_t size) {return this;}
    protected:
        Fn fn;
    };

    template<typename Fn>
    class ChainedCall: public SingleCall<Fn> {
    public:
        ChainedCall(Fn &&fn, Ptr &&next):SingleCall<Fn>(std::forward<Fn>(fn)),next(std::move(next)) {}
        virtual void run() noexcept {this->fn();next->run();}
        AbstractCall *move(char *buff, std::size_t &pos, std::size_t size) {return this;}
    protected:
        Ptr next;
    };

    template<typename Fn>
    class SingleCallNoAlloc: public SingleCall<Fn> {
    public:
        using SingleCall<Fn>::SingleCall;
        void operator delete(void *ptr, std::size_t sz) {}
        AbstractCall *move(char *buff, std::size_t &pos, std::size_t size) {
            auto sz = sizeof(SingleCallNoAlloc<Fn>);
            AbstractCall *out;
            if (pos+sz > size) {
                out = new SingleCall<Fn>(std::move(this->fn));
            } else {
                void *p = buff+pos;
                pos+=sz;
                out = new(p) SingleCallNoAlloc(std::move(this->fn));
            }
            delete this;
            return out;
        }
    };
    template<typename Fn>
    class ChainedCallNoAlloc: public ChainedCall<Fn> {
    public:
        using ChainedCall<Fn>::ChainedCall;
        void operator delete(void *ptr, std::size_t sz) {}
        AbstractCall *move(char *buff, std::size_t &pos, std::size_t size) {
            auto sz = sizeof(ChainedCallNoAlloc<Fn>);
            AbstractCall *out;
            if (pos+sz > size) {
                out =  new ChainedCall<Fn>(std::move(this->fn), Ptr(this->next.release()->move(buff, pos, size)));
            } else {
                void *p = buff+pos;
                pos+=sz;
                out =  new(p) ChainedCallNoAlloc(std::move(this->fn), Ptr(this->next.release()->move(buff, pos, size)));
            }
            delete this;
            return out;
        }
    };

};


template<std::size_t buffsz>
class Trailer: public Trailer_Defs {
public:

    ///Construct empty trailer
    /**
     * @see push
     */
    Trailer() {};
    Trailer(const Trailer &&other) = delete;
    Trailer &operator=(Trailer &&other) = delete;
    Trailer &operator=(const Trailer &&other) = delete;
    ~Trailer();

    Trailer(Trailer &&other);

    ///Push trailer (to the trailer stack)
    /**
     * @param fn function to execute as trailer <void()>
     *
     * @note Trailers are executed in reversed order (LIFO)
     */
    template<typename Fn, typename = decltype(std::declval<Fn>()())>
    void push(Fn &&fn);

    ///Construct trailer object and push trailer
    /**
     * @param fn function to execute as trailer <void()>
     * @see push
     */
    template<typename Fn, typename = decltype(std::declval<Fn>()())>
    Trailer(Fn &&fn){
        push(std::forward<Fn>(fn));
    }

    ///Clear all scheduled trailers
    /**
     * Function removes all scheduled trailers.
     */
    void clear();

    template<typename Fn>
    friend auto trailer(Fn &&fn);

 protected:
    Ptr ptr;
    char buffer[buffsz];
    std::size_t aptr = 0;

    void *alloc(std::size_t size) {
        if (aptr+size > buffsz) return nullptr;
        void *out = buffer+aptr;
        aptr+=size;
        return out;
    }



};
template<std::size_t buffsz>
template<typename Fn, typename >
void Trailer<buffsz>::push(Fn &&fn) {
    if (ptr == nullptr)  {
        std::size_t needsz = sizeof(SingleCallNoAlloc<Fn>);
        void *a = alloc(needsz);
        if (a == nullptr) {
            ptr = std::make_unique<SingleCall<Fn> >(std::forward<Fn>(fn));
        } else {
            ptr = Ptr(new(a) SingleCallNoAlloc<Fn>(std::forward<Fn>(fn)));
        }
    } else {
        std::size_t needsz = sizeof(ChainedCallNoAlloc<Fn>);
        void *a = alloc(needsz);
        if (a == nullptr) {
            ptr = std::make_unique<ChainedCall<Fn> >(std::forward<Fn>(fn), std::move(ptr));
        } else {
            ptr = Ptr(new(a) ChainedCallNoAlloc<Fn>(std::forward<Fn>(fn), std::move(ptr)));
        }
    }
}

template<std::size_t buffsz>
inline Trailer<buffsz>::~Trailer() {
    if (ptr != nullptr) ptr->run();
}

template<std::size_t buffsz>
inline Trailer<buffsz>::Trailer(Trailer &&other) {
    ptr = Ptr(other.ptr.release()->move(buffer, aptr, buffsz));
}

template<std::size_t buffsz>
inline void Trailer<buffsz>::clear() {
    ptr = nullptr;
    aptr = 0;
}

template<typename Fn>
auto trailer(Fn &&fn) {
    return Trailer<sizeof(Trailer_Defs::SingleCallNoAlloc<Fn>)>(std::forward<Fn>(fn));
}




}


#endif /* SRC_ONDRA_SHARED_TRAILER_H_6765144087064 */
