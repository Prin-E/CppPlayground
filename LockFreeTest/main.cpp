#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
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

void lock_free_stack_thread_push_main(lf_default_stack *stack, int thread_index, int num_iteration, int *value_log) {
    for(int i = 0; i < num_iteration; i++) {
        int value = thread_index * num_iteration + i;
        stack->push(value);
        value_log[i] = value;
    }
}

void lock_free_stack_thread_pop_main(lf_default_stack *stack, int thread_index, int num_iteration, int *value_log) {
    int value = 0;
    for(int i = 0; i < num_iteration; i++) {
        while(!stack->pop(value));
        value_log[i] = value;
    }
}

void lock_free_stack_thread_push_stress_main(lf_default_stack *stack, int thread_index, int num_iteration) {
    for(int i = 0; i < num_iteration; i++) {
        stack->push(rand()%10000);
    }
}

void lock_free_stack_thread_pop_stress_main(lf_default_stack *stack, int thread_index, int num_iteration) {
    int value = 0;
    for(int i = 0; i < num_iteration; i++) {
        while(!stack->pop(value));
    }
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
        lf_default_stack stack;
        
        // single threaded test
        {
            std::cout << "Single-threaded push and pop test..." << std::endl;
            
            // push
            stack.push(3);
            stack.push(4);
            stack.push(2);
            
            // pop
            int val = 0;
            while(stack.pop(val))
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
                ts_push[k] = std::thread(lock_free_stack_thread_push_main, &stack, k, num_push_iteration, push_value_log[k]);
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
            constexpr int num_push_threads = 1000;
            constexpr int num_pop_threads = 1000;
            constexpr int num_push_iteration = 2000000;
            constexpr int num_pop_iteration = 2000000;
            static_assert(num_push_threads * num_push_iteration == num_pop_threads * num_pop_iteration, "push and pop count are mismatch!");
            
            int *push_value_log[num_push_threads];
            int *pop_value_log[num_pop_threads];
            for(int i = 0; i < num_push_threads; i++) {
                push_value_log[i] = new int[num_push_iteration];
            }
            for(int i = 0; i < num_pop_threads; i++) {
                pop_value_log[i] = new int[num_pop_iteration];
            }
            
            std::cout << "Running " << (num_push_threads + num_pop_threads) << " threads... (iteration:push(" << num_push_iteration << "),pop(" << num_pop_iteration << "))" << std::endl;
            
            std::chrono::time_point time_begin = std::chrono::system_clock::now();
            
            std::thread ts_push[num_push_threads];
            std::thread ts_pop[num_pop_threads];
            for(int k = 0; k < num_push_threads; k++) {
                ts_push[k] = std::thread(lock_free_stack_thread_push_main, &stack, k, num_push_iteration, push_value_log[k]);
            }
            for(int k = 0; k < num_pop_threads; k++) {
                ts_pop[k] = std::thread(lock_free_stack_thread_pop_main, &stack, k, num_pop_iteration, pop_value_log[k]);
            }
            for(int k = 0; k < num_push_threads; k++) {
                ts_push[k].join();
            }
            for(int k = 0; k < num_pop_threads; k++) {
                ts_pop[k].join();
            }
            std::cout << "--------------------------------" << std::endl;
            
            std::chrono::time_point time_to = std::chrono::system_clock::now();
            std::chrono::duration<double> elapsed = (time_to - time_begin);
            std::cout << "Elapsed : " << elapsed.count() << " sec" << std::endl;
            
            // validate push-pop values
            std::cout << "Validating..." << std::endl;
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
            
            if(push_vec == pop_vec) {
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
        
        // multi threaded test (stress)
        {
            std::cout << "Multi-threaded stress test..." << std::endl;
            constexpr int num_push_threads = 250;
            constexpr int num_pop_threads = 250;
            constexpr int num_iteration = 10000000;
            std::cout << "Running " << (num_push_threads + num_pop_threads) << " threads... (iteration:" << num_iteration << ")" << std::endl;
            
            std::thread ts_push[num_push_threads];
            std::thread ts_pop[num_pop_threads];
            for(int k = 0; k < num_push_threads; k++) {
                ts_push[k] = std::thread(lock_free_stack_thread_push_stress_main, &stack, k, num_iteration);
            }
            for(int k = 0; k < num_pop_threads; k++) {
                ts_pop[k] = std::thread(lock_free_stack_thread_pop_stress_main, &stack, k, num_iteration);
            }
            for(int k = 0; k < num_push_threads; k++) {
                ts_push[k].join();
            }
            for(int k = 0; k < num_pop_threads; k++) {
                ts_pop[k].join();
            }
            std::cout << "--------------------------------" << std::endl;
            std::cout << "Complete!" << std::endl << std::endl;
        }
    }
    
    return 0;
}
