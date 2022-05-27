/*
 * fastsharedalloc.h
 *
 *  Created on: 27. 5. 2022
 *      Author: ondra
 */

#ifndef SRC_SHARED_FASTSHAREDALLOC_H_
#define SRC_SHARED_FASTSHAREDALLOC_H_
#include <atomic>


namespace ondra_shared {

///Allocator optimized rapid allocation and deallocation many of small objects, such a callbacks
/**
 * Allocator uses cache of deallocated objects to reuse them during next allocation.
 * There are 16 caches. Size of objects are in granuality of 4x size of void (16b for 32bit,32b for 64bit).
 * Maximum cacheable block is 16*4*sizeof(void) (256b for 32bit, 512 for 64bit)
 *
 * There is no limit how many block can be cached. However cached block can occupy significant
 * part of memory. In this case, you can call ::gc() to free any cached memory.
 *
 * To use this object, you need just to inherit this object. Derived object is then able to use
 * customized new and delete operators
 *
 */
class FastSharedAlloc {


    static constexpr std::size_t levelStep = sizeof(void *)*4;

    static constexpr int sizeToLevel(std::size_t x) {
        return static_cast<int>((x + levelStep-1)/levelStep);
    }

    static constexpr std::size_t stepToLevel(int x) {
        return x = (x + levelStep-1)/levelStep;
    }

    struct BlockBase {
    };

    struct BlockBase2: public BlockBase {
        static constexpr BlockBase dummy = {};
        static constexpr BlockBase const *shutdown_flag = &dummy;
    };

    template<int level>
    struct Block: public BlockBase2 {
        static constexpr std::size_t blk_size = levelStep*level;
        Block *next = nullptr;
        char reserved[blk_size - sizeof(Block *)];
    };

    template<int level>
    class Chain {
    public:
        using Blk = Block<level>;
        using BlkPtr = Blk *;

        void *alloc() {
            BlkPtr nx = blk_stack.load();
            BlkPtr out;
            do {
                if (nx == Blk::shutdown_flag || nx == nullptr) {
                    return ::operator new(Blk::blk_size);
                }
                out = nx;

                //NOTE - this can cause invalid dereference error
                //when racing thread allocates and then deallocates
                //block faster, while allocator is shut down at the same time

                nx = nx->next;
            } while (!blk_stack.compare_exchange_weak(nx, out));
            return out;
        }

        void free(void *ptr) {
            BlkPtr p = reinterpret_cast<BlkPtr>(ptr);
            p->next = blk_stack.load();;
            if (p->next == Blk::shutdown_flag) {
                ::operator delete(ptr);
                return;
            }
            while(!blk_stack.compare_exchange_weak(p->next, p)) {
                if (p->next == Blk::shutdown_flag) {
                    ::operator delete(ptr);
                    return;
                }
            }
        }

        BlkPtr shutdown() {
            BlkPtr sht = const_cast<BlkPtr>(static_cast<const Blk *>(Blk::shutdown_flag));
            return blk_stack.exchange(sht);
        }

        void restart() {
            BlkPtr sht = const_cast<BlkPtr>(static_cast<const Blk *>(Blk::shutdown_flag));
            blk_stack.compare_exchange_strong(sht, nullptr);
        }

        static void freeChain(BlkPtr x) {
            while (x) {
                BlkPtr z = x;
                x=x->next;
                ::operator delete(z);
            }
        }

    protected:
        std::atomic<BlkPtr> blk_stack;
    };

    class Allocator {
    public:


        Chain<1> blk01;
        Chain<2> blk02;
        Chain<3> blk03;
        Chain<4> blk04;
        Chain<5> blk05;
        Chain<6> blk06;
        Chain<7> blk07;
        Chain<8> blk08;
        Chain<9> blk09;
        Chain<10> blk10;
        Chain<11> blk11;
        Chain<12> blk12;
        Chain<13> blk13;
        Chain<14> blk14;
        Chain<15> blk15;
        Chain<16> blk16;

        template<typename Fn>
        bool chooseChain(int id, Fn &&fn) {
            switch (id) {
            case 1: fn(blk01); return true;
            case 2: fn(blk02); return true;
            case 3: fn(blk03); return true;
            case 4: fn(blk04); return true;
            case 5: fn(blk05); return true;
            case 6: fn(blk06); return true;
            case 7: fn(blk07); return true;
            case 8: fn(blk08); return true;
            case 9: fn(blk09); return true;
            case 10: fn(blk10); return true;
            case 11: fn(blk11); return true;
            case 12: fn(blk12); return true;
            case 13: fn(blk13); return true;
            case 14: fn(blk14); return true;
            case 15: fn(blk15); return true;
            case 16: fn(blk16); return true;
            default: return false;
            }
        }

        ~Allocator() {
            shutdown();
        }

        ///shutdown allocator
        /** Method itself is not MT safe, it is MT safe relative to alloc/dealloc */
        void shutdown() {
            void *chains[16];
            //collect all chains and shutdown caches
            for (int i = 0; i < 16; i++) {
                chooseChain(i+1, [&](auto &c){chains[i] = c.shutdown();});
            }
            //this order was choosen to minimize possibility of a race condition.
            //It there is racing thread allocating
            //a block, it can still receive block from chain, before the chain is shut down. The
            //received block still have a valid next pointer for a while. It is expected, that racing
            //thread fails to compare_exchange and finds chain shutted down.

            //during shutdown, memory fence is issued many times

            //After all chains are shutted down, release the rest of memory
            for (int i = 0; i < 16; i++) {
                chooseChain(i+1, [&](auto &c) {
                    using T = typename std::remove_reference<decltype(c)>::type;
                    using Ptr = typename T::BlkPtr;
                    T::freeChain(reinterpret_cast<Ptr>(chains[i]));
                });
            }
        }

        ///free extra memory
        /** Method itself is not MT safe, it is MT safe relative to alloc/dealloc */
        void gc() {
            //Garbage collector, shuts down the cache, to prevent allocation from cache during releasing
            //the cached memory
            shutdown();
            //when memory is released, restart the caches
            for (int i = 0; i < 16; i++) {
                chooseChain(i+1, [&](auto &c){c.restart();});
            }
        }

        ///Alloate memory - method is MT safe (lockfree)
        void *alloc(std::size_t size) {
            if (size == 0) size = 1;
            int level = sizeToLevel(size);
            void *out;
            if (!chooseChain(level, [&](auto &block){
                out = block.alloc();
            })) {
                out = ::operator new(size);
            }
            return out;
        }

        ///Free memory - method is MT safe (lockfree)
        void free(void *ptr, std::size_t size) {
            if (size == 0) size = 1;
            int level = sizeToLevel(size);
            if (!chooseChain(level, [&](auto &block){
                block.free(ptr);
            })) {
                ::operator delete(ptr);
            }
        }
    };

    static Allocator &get_instance() {
        static Allocator allocator;
        return allocator;
    }
public:

    static void gc() {
        return get_instance().gc();
    }

    void *operator new(std::size_t sz) {
        return get_instance().alloc(sz);
    }

    void operator delete(void *ptr, std::size_t sz) {
        return get_instance().free(ptr,sz);
    }
};



}


#endif /* SRC_SHARED_FASTSHAREDALLOC_H_ */
