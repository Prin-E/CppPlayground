//
//  Mutex.h
//  CppPlayground
//
//  Created by 이현우 on 2021/10/28.
//

#ifndef Mutex_h
#define Mutex_h

#include <atomic>

// number of spins
#define DEFAULT_SPIN_COUNT 512

// flag to use x64 BTS assembly in GCC/Clang (12~20% faster than XCHG!)
#define USE_GCC_ASSEMBLY_X64_BTS (__x86_64__ && __GNUC__)

// scoped lock
template<typename mutex_t>
class scoped_lock {
public:
    scoped_lock(mutex_t *in_mutex) : mutex(in_mutex) { mutex->lock(); }
    ~scoped_lock() { mutex->unlock(); }
    
private:
    mutex_t *mutex;
};

// spinlock mutex
class spinlock_mutex {
public:
    spinlock_mutex(int new_spin_count = DEFAULT_SPIN_COUNT) : spin_count(new_spin_count) {}
    
    inline void lock() {
#if USE_GCC_ASSEMBLY_X64_BTS
        mutex_try:
        uint8_t ret;
        asm (
            "lock btsl $0, %0\n\t"
            "setb %1\n\t"
            : "+m" (flag), "=r" (ret)
        );
        
        if(ret) {
#else
        while(flag.test_and_set(std::memory_order_seq_cst)) {
#endif
            for(int i = 0; i < spin_count; i++) {
                std::this_thread::yield();
            }
        
#if USE_GCC_ASSEMBLY_X64_BTS
            goto mutex_try;
#endif
        }
    }
    
    inline void unlock() {
#if USE_GCC_ASSEMBLY_X64_BTS
        asm (
            "lock btrl $0, %0\n\t"
            : "+m" (flag)
        );
#else
        flag.clear(std::memory_order_seq_cst);
#endif
    }
    
private:
#if USE_GCC_ASSEMBLY_X64_BTS
    alignas(64) volatile int flag = 0;
#else
    alignas(64) std::atomic_flag flag = ATOMIC_FLAG_INIT;
#endif
    alignas(64) int spin_count;
};

// spinlock critical section
class spinlock_critical_section {
public:
    spinlock_critical_section(int new_spin_count = DEFAULT_SPIN_COUNT) : spin_count(new_spin_count), enter_count(0) {}
    
    void lock() {
        auto this_thread_id = std::this_thread::get_id();
        if(thread_id == this_thread_id) {
            enter_count++;
            return;
        }

        while(flag.test_and_set(std::memory_order_seq_cst)) {
            for(int i = 0; i < spin_count; i++) {
                std::this_thread::yield();
            }
        }
        thread_id = this_thread_id;
        enter_count = 1;
    }
    
    void unlock() {
        enter_count--;
        if(enter_count == 0) {
            thread_id = std::thread::id();
            flag.clear(std::memory_order_seq_cst);
        }
    }
    
private:
    alignas(64) std::atomic_flag flag = ATOMIC_FLAG_INIT;
    alignas(64) int spin_count;
    alignas(64) std::thread::id thread_id;
    int enter_count;
};

#endif /* Mutex_h */
