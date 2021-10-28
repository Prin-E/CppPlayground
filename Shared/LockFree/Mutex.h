//
//  Mutex.h
//  CppPlayground
//
//  Created by 이현우 on 2021/10/28.
//

#ifndef Mutex_h
#define Mutex_h

#include <atomic>

// spinlock mutex using atomic flag
class spinlock_mutex {
public:
    void lock() {
        while(flag.test_and_set(std::memory_order_acquire));
    }
    
    void unlock() {
        flag.clear(std::memory_order_release);
    }
    
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
};


#endif /* Mutex_h */
