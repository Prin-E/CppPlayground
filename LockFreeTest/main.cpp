#include <iostream>
#include <thread>
#include "../Shared/Shared.h"

// global variables
spinlock_mutex global_mutex;
std::atomic<int> global_intval{0};

// thread main
void atomic_flag_thread_main() {
    global_mutex.lock();
    volatile int val = global_intval.fetch_add(1, std::memory_order_relaxed) + 1;
    std::cout << "Called from thread! (counter:" << val << ")" << std::endl;
    global_mutex.unlock();
}

int main(int argc, const char * argv[]) {
    // test flags
    constexpr bool test_atomic_flag = true;
    
    // atomic_flag
    if(test_atomic_flag)
    {
        spinlock_mutex mutex;
        mutex.lock();
        mutex.unlock();
        
        // duplicated lock (deadlock)
        /*
        mutex.lock();
        mutex.lock();
        mutex.unlock();
        mutex.unlock();
         */
        
        // multiple threads
        constexpr int num_threads = 8;
        constexpr int num_iteration = 10;
        std::cout << "Running " << num_threads << " threads... (iteration:" << num_iteration << ")" << std::endl;
        for(int i = 0; i < num_iteration; i++) {
            std::cout << "--------------------------------" << std::endl;
            std::cout << "- Iteration " << (i + 1) << std::endl;
            std::thread ts[num_threads];
            for(int k = 0; k < num_threads; k++) {
                ts[k] = std::thread(atomic_flag_thread_main);
            }
            for(int k = 0; k < num_threads; k++) {
                ts[k].join();
            }
        }
        std::cout << "--------------------------------" << std::endl;
        std::cout << "Complete!" << std::endl;
    }
    
    return 0;
}
