//
//  LockFreeQueue.h
//  CppPlayground
//
//  Created by 이현우 on 2023/03/16.
//

#ifndef LockFreeQueue_h
#define LockFreeQueue_h

#include <atomic>
#include <memory>
#include <utility>
#include "../../Option/Option.h"

// Lock-free queue
template<typename T, bool MPMC>
class lf_queue;

// Lock-free queue (single-producer, single-consumer)
template<typename T>
class lf_queue<T, false> {
public:
    lf_queue() {
        node_t *node = new node_t();
        node_link_t node_link;
        node_link.ptr = (uintptr_t)node;
        head.store(node_link, std::memory_order_release);
        tail.store(node_link, std::memory_order_release);
    }
    
    ~lf_queue() {
        T value;
        while(pop(value));
        node_link_t tail_link = tail.load(std::memory_order_acquire);
        node_t *node = (node_t*)tail_link.ptr;
        delete node;
    }
    
    void push(const T &value) {
        node_t *new_tail = new node_t();
        node_link_t new_tail_link;
        new_tail_link.ptr = (uintptr_t)new_tail;
        new_tail_link.counter = 1;
        
        node_link_t prev_tail_link = tail.load(std::memory_order_acquire);
        node_t *prev_tail = (node_t*)prev_tail_link.ptr;
        prev_tail->value = value;
        prev_tail->next = new_tail_link;
        prev_tail->ref_count = 1;
        
        tail.store(new_tail_link, std::memory_order_release);
    }
    
    bool pop(T &out_value) {
        node_link_t head_link = head.load(std::memory_order_relaxed);
        node_link_t tail_link = tail.load(std::memory_order_relaxed);
        if(head_link.ptr != tail_link.ptr) {
            node_t *node = (node_t*)head_link.ptr;
            node_link_t new_head_link = node->next;
            if(head.compare_exchange_strong(head_link,
                                            new_head_link,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed)) {
                out_value = node->value;
                delete node;
                return true;
            }
        }
        return false;
    }
    
private:
    // internal node link
    struct node_link_t {
        union {
            uintptr_t value;
            struct {
                // assumes that the 64-bit system has 52-bit or lower address space.
                uintptr_t ptr : 52;
                // counter variable to prevent ABA problem (0~4096)
                uintptr_t counter : 12;
            };
        };
        node_link_t() : value(0) {}
    };
    
    struct node_t {
        node_link_t next;
        std::atomic<uint32_t> ref_count;
        T value;
        
#if USE_MEMORY_POOL
        static void *operator new(size_t size) {
            return global_memory_pool.allocate(size);
        }
        
        static void operator delete(void *ptr) {
            global_memory_pool.free(ptr);
        }
#endif
    };
    
    std::atomic<node_link_t> head;
    std::atomic<node_link_t> tail;
};

// Lock-free queue (multiple-producer, multiple-consumer)
template<typename T>
class lf_queue<T, true> {
public:
    lf_queue() {
        node_t *node = new node_t();
        node_link_t node_link;
        node_link.ptr = (uintptr_t)node;
        head.store(node_link, std::memory_order_release);
        tail.store(node_link, std::memory_order_release);
    }
    
    ~lf_queue() {
        T value;
        while(pop(value));
        node_link_t tail_link = tail.load(std::memory_order_acquire);
        node_t *node = (node_t*)tail_link.ptr;
        delete node;
    }
    
    void push(const T &value) {
        node_t *new_tail = new node_t();
        node_link_t new_tail_link;
        new_tail_link.ptr = (uintptr_t)new_tail;
        new_tail_link.counter = 1;

        while(true) {
            node_link_t prev_tail_link = tail.load(std::memory_order_relaxed);
            node_link_t link;
            do {
                link = prev_tail_link;
                link.counter++;
            }
            while(!tail.compare_exchange_strong(prev_tail_link, link, std::memory_order_acquire, std::memory_order_relaxed));

            node_t *old_tail = (node_t*)link.ptr;
            bool expected_value_flag = false;
            if(old_tail->value_flag.compare_exchange_strong(expected_value_flag, true, std::memory_order_acquire, std::memory_order_relaxed)) {
                old_tail->value = value;
                old_tail->next = new_tail_link;
                tail.exchange(new_tail_link, std::memory_order_acq_rel);
                old_tail->release_counter(2 - static_cast<int32_t>(link.counter), 1);
                break;
            }
            else {
                old_tail->release_counter(1);
            }
        }
    }
    
    bool pop(T &out_value) {
        node_link_t link;
        node_link_t prev_head_link = head.load(std::memory_order_relaxed);

        while(true) {
            do {
                link = prev_head_link;
                link.counter++;
            }
            while(!head.compare_exchange_strong(prev_head_link, link, std::memory_order_acquire, std::memory_order_relaxed));

            node_t *node = (node_t*)link.ptr;
            node_link_t prev_tail_link = tail.load(std::memory_order_relaxed);
            if(link.ptr == prev_tail_link.ptr) {
                node->release_counter(1);
                return false;
            }
            if(head.compare_exchange_strong(link, node->next, std::memory_order_acquire, std::memory_order_relaxed)) {
                out_value = node->value;
                node->release_counter(2 - static_cast<int32_t>(link.counter), 1);
                return true;
            }
            node->release_counter(1);
        }
        return false;
    }
    
private:
    // internal node link
    struct node_link_t {
        union {
            uintptr_t value;
            struct {
                // assumes that the 64-bit system has 52-bit or lower address space.
                uintptr_t ptr : 52;
                // counter variable to prevent ABA problem (0~4096)
                uintptr_t counter : 12;
            };
        };
        node_link_t() : value(0) {}
    };

    // internal node counter
    struct node_counter_t {
        union {
            uint32_t value;
            struct {
                int32_t counter : 30;
                uint32_t external_counters : 2;
            };
        };
        node_counter_t() : counter(0), external_counters(2) {}
    };
    
    // internal node structure
    struct node_t {
        node_link_t next;
        std::atomic<node_counter_t> ref_count;
        std::atomic<bool> value_flag;
        T value;
        
#if USE_MEMORY_POOL
        static void *operator new(size_t size) {
            return global_memory_pool.allocate(size);
        }
        
        static void operator delete(void *ptr) {
            global_memory_pool.free(ptr);
        }
#endif

        node_counter_t release_counter(int32_t counter, uint32_t external_counters = 0) {
            node_counter_t old_ref_count = ref_count.load(std::memory_order_relaxed);
            node_counter_t new_ref_count;
            do {
                new_ref_count = old_ref_count;
                new_ref_count.counter -= counter;
                new_ref_count.external_counters -= external_counters;
            }
            while(ref_count.compare_exchange_strong(old_ref_count, new_ref_count, std::memory_order_acquire, std::memory_order_relaxed));
            if(new_ref_count.value == 0) {
                delete this;
            }
            return new_ref_count;
        }
    };
    
    std::atomic<node_link_t> head;
    std::atomic<node_link_t> tail;
};

typedef lf_queue<int, true> lf_default_queue;

#endif /* LockFreeQueue_h */
