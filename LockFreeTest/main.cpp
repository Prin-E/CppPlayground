#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include "../Platform/Platform.h"
#include "../Shared/Shared.h"

// global variables
spinlock_mutex global_mutex;
std::atomic<int> global_intval{0};

// typedefs
typedef platform::platform_lf_stack<int> platform_lf_stack_t;
typedef lf_default_stack lf_stack_t;

// thread main
void atomic_flag_thread_main() {
    global_mutex.lock();
    volatile int val = global_intval.fetch_add(1, std::memory_order_relaxed) + 1;
    std::cout << "Called from thread! (counter:" << val << ")" << std::endl;
    global_mutex.unlock();
}

template <typename T>
void lock_free_stack_thread_push_main(T *stack, int thread_index, int num_iteration, int *value_log) {
    for(int i = 0; i < num_iteration; i++) {
        int value = thread_index * num_iteration + i;
        stack->push(value);
        value_log[i] = value;
    }
}
template void lock_free_stack_thread_push_main<platform_lf_stack_t>(platform_lf_stack_t*, int, int, int*);
template void lock_free_stack_thread_push_main<lf_stack_t>(lf_stack_t*, int, int, int*);

template <typename T>
void lock_free_stack_thread_pop_main(T *stack, int thread_index, int num_iteration, int *value_log) {
    int value = 0;
    for(int i = 0; i < num_iteration; i++) {
        while(!stack->pop(value));
        value_log[i] = value;
    }
}

template void lock_free_stack_thread_pop_main<platform_lf_stack_t>(platform_lf_stack_t*, int, int, int*);
template void lock_free_stack_thread_pop_main<lf_stack_t>(lf_stack_t*, int, int, int*);

// validation
bool validate_push_pop(int **push_value_log, int **pop_value_log, int num_push_threads, int num_push_iteration, int num_pop_threads, int num_pop_iteration) {
    std::vector<int> push_vec, pop_vec;
    for(int i = 0; i < num_push_threads; i++) {
        for(int j = 0; j < num_push_iteration; j++)
            push_vec.push_back(push_value_log[i][j]);
    }
    for(int i = 0; i < num_pop_threads; i++) {
        for(int j = 0; j < num_pop_iteration; j++)
            pop_vec.push_back(pop_value_log[i][j]);
    }
    //std::sort(push_vec.begin(), push_vec.end());
    std::sort(pop_vec.begin(), pop_vec.end());
    return push_vec == pop_vec;
}

int main(int argc, const char * argv[]) {
    // test flags
    constexpr bool test_atomic_flag = false;
    constexpr bool test_lf_stack = true;
    
    // atomic_flag
    if(test_atomic_flag) {
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
    
    if(test_lf_stack) {
        lf_stack_t stack;
        platform_lf_stack_t platform_stack;
        
        // single threaded test
        {
            std::cout << "Single-threaded push and pop test..." << std::endl;
            int val;
            
            // push
            stack.push(3);
            stack.push(4);
            stack.push(2);
            
            // pop
            while(stack.pop(val))
                std::cout << val << std::endl;
            
            // push
            platform_stack.push(3);
            platform_stack.push(4);
            platform_stack.push(2);
            
            // pop
            while(platform_stack.pop(val))
                std::cout << val << std::endl;
            
            std::cout << "--------------------------------" << std::endl;
            std::cout << "Complete!" << std::endl << std::endl;
            
            STAT_ALIVE_NODE_COUNT;
        }
        
        // multi threaded test (push validation)
        {
            std::cout << "Multi-threaded push test..." << std::endl;
            constexpr int num_push_threads = 32;
            constexpr int num_push_iteration = 1000;
            int push_value_log[num_push_threads][num_push_iteration];
            int stack_values[num_push_threads * num_push_iteration];
            uintptr_t stack_counters[num_push_threads * num_push_iteration];
            
            std::cout << "Running " << num_push_threads << " threads... (iteration:" << num_push_iteration << ")" << std::endl;
            
            std::thread ts_push[num_push_threads];
            for(int k = 0; k < num_push_threads; k++) {
                ts_push[k] = std::thread(lock_free_stack_thread_push_main<lf_stack_t>, &stack, k, num_push_iteration, push_value_log[k]);
            }
            for(int k = 0; k < num_push_threads; k++) {
                ts_push[k].join();
            }
            std::cout << "--------------------------------" << std::endl;
            
            // validate pushed values
            std::cout << "Validating..." << std::endl;
            std::vector<int> push_vec, stack_values_vec;
            std::vector<uintptr_t> stack_counters_vec;
            for(int i = 0; i < num_push_threads; i++) {
                push_vec.insert(push_vec.end(), std::begin(push_value_log[0]), std::end(push_value_log[0]));
            }
            stack.debug_fetch(stack_values, stack_counters, num_push_threads * num_push_iteration);
            stack_values_vec.insert(stack_values_vec.end(), std::begin(stack_values), std::end(stack_values));
            stack_counters_vec.insert(stack_counters_vec.end(), std::begin(stack_counters), std::end(stack_counters));
            std::sort(stack_values_vec.begin(), stack_values_vec.end());
            std::sort(stack_counters_vec.begin(), stack_counters_vec.end());

            int validation_value = -1;
            bool validation_flag = true;
            for(size_t i = 0; i < stack_values_vec.size(); i++) {
                int value = stack_values_vec.at(i);
                if(validation_value + 1 != value) {
                    validation_flag = false;
                    break;
                }
                validation_value = value;
            }
            
            if(validation_flag)
                std::cout << "Validation success!" << std::endl;
            else
                std::cout << "Validation failed!" << std::endl;
            
            std::cout << "Complete!" << std::endl << std::endl;
            
            // pops all nodes in stack
            int dummy_val;
            while(stack.pop(dummy_val)) {}
            
            STAT_ALIVE_NODE_COUNT;
        }
        
        // multi threaded test (push-pop validation)
        {
            std::cout << "Multi-threaded push and pop test..." << std::endl;

            // If we set too many number of threads, IDE hangs on M1. :'(
            constexpr int num_push_threads = 32;
            constexpr int num_pop_threads = 32;
            constexpr int num_push_iteration = 1000000;
            constexpr int num_pop_iteration = 1000000;
            static_assert(num_push_threads * num_push_iteration == num_pop_threads * num_pop_iteration, "push and pop count are mismatch!");
            
            int *push_value_log[num_push_threads];
            int *pop_value_log[num_pop_threads];
            for(int i = 0; i < num_push_threads; i++) {
                push_value_log[i] = new int[num_push_iteration];
            }
            for(int i = 0; i < num_pop_threads; i++) {
                pop_value_log[i] = new int[num_pop_iteration];
            }
            
            std::chrono::time_point<std::chrono::system_clock> time_begin, time_to;
            std::chrono::duration<double> elapsed;
            std::thread ts_push[num_push_threads];
            std::thread ts_pop[num_pop_threads];
            
            /* platform lock-free stack (refernce) */
            std::cout << "(platform) Running " << (num_push_threads + num_pop_threads) << " threads... (iteration:push(" << num_push_iteration << "),pop(" << num_pop_iteration << "))" << std::endl;
            time_begin = std::chrono::system_clock::now();
            
            for(int k = 0; k < num_push_threads; k++) {
                ts_push[k] = std::thread(lock_free_stack_thread_push_main<platform_lf_stack_t>, &platform_stack, k, num_push_iteration, push_value_log[k]);
            }
            for(int k = 0; k < num_pop_threads; k++) {
                ts_pop[k] = std::thread(lock_free_stack_thread_pop_main<platform_lf_stack_t>, &platform_stack, k, num_pop_iteration, pop_value_log[k]);
            }
            for(int k = 0; k < num_push_threads; k++) {
                ts_push[k].join();
            }
            for(int k = 0; k < num_pop_threads; k++) {
                ts_pop[k].join();
            }
            std::cout << "--------------------------------" << std::endl;
            
            time_to = std::chrono::system_clock::now();
            elapsed = (time_to - time_begin);
            std::cout << "Elapsed : " << elapsed.count() << " sec" << std::endl;
            
            // validate push-pop values
            std::cout << "Validating..." << std::endl;
            if(validate_push_pop(push_value_log, pop_value_log, num_push_threads, num_push_iteration, num_pop_threads, num_pop_iteration)) {
                std::cout << "Validation success!" << std::endl;
            }
            else {
                std::cout << "Validation failed!" << std::endl;
            }
            
            /* lock-free stack using std::atomic */
            std::cout << "(std::atomic) Running " << (num_push_threads + num_pop_threads) << " threads... (iteration:push(" << num_push_iteration << "),pop(" << num_pop_iteration << "))" << std::endl;
            time_begin = std::chrono::system_clock::now();
            
            for(int k = 0; k < num_push_threads; k++) {
                ts_push[k] = std::thread(lock_free_stack_thread_push_main<lf_stack_t>, &stack, k, num_push_iteration, push_value_log[k]);
            }
            for(int k = 0; k < num_pop_threads; k++) {
                ts_pop[k] = std::thread(lock_free_stack_thread_pop_main<lf_stack_t>, &stack, k, num_pop_iteration, pop_value_log[k]);
            }
            for(int k = 0; k < num_push_threads; k++) {
                ts_push[k].join();
            }
            for(int k = 0; k < num_pop_threads; k++) {
                ts_pop[k].join();
            }
            std::cout << "--------------------------------" << std::endl;
            
            time_to = std::chrono::system_clock::now();
            elapsed = (time_to - time_begin);
            std::cout << "Elapsed : " << elapsed.count() << " sec" << std::endl;
            
            // validate push-pop values
            std::cout << "Validating..." << std::endl;
            if(validate_push_pop(push_value_log, pop_value_log, num_push_threads, num_push_iteration, num_pop_threads, num_pop_iteration)) { 
                std::cout << "Validation success!" << std::endl;
            }
            else {
                std::cout << "Validation failed!" << std::endl;
            }
            
            for(int i = 0; i < num_push_threads; i++) {
                delete push_value_log[i];
            }
            for(int i = 0; i < num_pop_threads; i++) {
                delete pop_value_log[i];
            }
            
            std::cout << "Complete!" << std::endl << std::endl;
            
            STAT_ALIVE_NODE_COUNT;
        }
    }
    
    return 0;
}
