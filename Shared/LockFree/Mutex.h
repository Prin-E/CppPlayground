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

// scoped lock
template<typename mutex_t>
class scoped_lock {
public:
    scoped_lock(mutex_t *in_mutex) : mutex(in_mutex) { mutex->lock(); }
    ~scoped_lock() { mutex->unlock(); }
    
private:
    mutex_t *mutex;
};

// spinlock mutex using atomic flag
class spinlock_mutex {
public:
    spinlock_mutex(int new_spin_count = DEFAULT_SPIN_COUNT) : spin_count(new_spin_count) {}
    
    inline void lock() {
        while(flag.test_and_set(std::memory_order_seq_cst)) {
            for(int i = 0; i < spin_count; i++) {
                std::this_thread::yield();
            }
        }
    }
    
    inline void unlock() {
        flag.clear(std::memory_order_seq_cst);
    }
    
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
    int spin_count;
    int spin;
};


#endif /* Mutex_h */
