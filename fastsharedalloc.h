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
        static constexpr BlockBase dummy_shutdown = {};
        static constexpr BlockBase dummy_busy = {};
        static constexpr BlockBase const *shutdown_flag = &dummy_shutdown;
        static constexpr BlockBase const *busy_flag = &dummy_busy;
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
            //lock the queue - putting busy flag there
            BlkPtr busy_flg = static_cast<BlkPtr>(const_cast<BlockBase *>(BlockBase2::busy_flag));
            //also unshare current chain (so nobody will access it)
            BlkPtr nx = blk_stack.exchange(busy_flg);
            //if we got the busy_flag, queue is already locked, wait until unlocked - spinlock
            while (nx == busy_flg) {
                //put busy flag to the queue and receive content
                nx = blk_stack.exchange(busy_flg);
            }
            //if received shutdown flag or nullptr
            if (nx ==Blk::shutdown_flag || nx == nullptr) {
                //put it back (if there is still busy_flg)
                blk_stack.compare_exchange_strong(busy_flg, nx);
                //do standard allocation
                return ::operator new(Blk::blk_size);
            }
            //store top most block
            BlkPtr out = nx;
            //make new root (of next)
            nx = out->next;
            //put the new root to the top - there should be busy flag a the time;
            if (!blk_stack.compare_exchange_strong(busy_flg, nx)) {
                //if there were something else, we cannot put rest of list back
                //free rest of blocks
                while (nx) {
                    BlkPtr d = nx;
                    nx = nx->next;
                    ::operator delete(d);
                }
            }
            //return our block
            return out;
        }

        void free(void *ptr) {
            //convert ptr to block
            BlkPtr p = reinterpret_cast<BlkPtr>(ptr);
            //load top of chain
            p->next = blk_stack.load();;
            do {
                //if top is shutdown, we need to deallocate by operator
                if (p->next == Blk::shutdown_flag) {
                    ::operator delete(ptr);
                    return;
                }
                //if top is busy, we must wait in spin lock, so replace next with nullptr
                if (p->next == Blk::busy_flag) {
                    p->next = nullptr;
                }
                //try to replace top with out block
            } while(!blk_stack.compare_exchange_weak(p->next, p));
        }

        void shutdown() {
            BlkPtr sht = const_cast<BlkPtr>(static_cast<const Blk *>(Blk::shutdown_flag));
            BlkPtr x = blk_stack.exchange(sht);
            //if we received busy_flag - some other thread is responsible to free the chain
            if (x != Blk::busy_flag || x != Blk::shutdown_flag) {
                while (x) {
                    BlkPtr z = x;
                    x=x->next;
                    ::operator delete(z);
                }
            }

        }

        void restart() {
            BlkPtr sht = const_cast<BlkPtr>(static_cast<const Blk *>(Blk::shutdown_flag));
            blk_stack.compare_exchange_strong(sht, nullptr);
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
            for (int i = 0; i < 16; i++) {
                chooseChain(i+1, [&](auto &c){c.shutdown();});
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
