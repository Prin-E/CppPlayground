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
// TODO: currently SPSC, improve it to MPMC later.
template<typename T>
class lf_queue {
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
        while(dequeue(value));
        node_link_t tail_link = tail.load(std::memory_order_acquire);
        node_t *node = (node_t*)tail_link.ptr;
        delete node;
    }
    
    void enqueue(const T &value) {
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
    
    bool dequeue(T &out_value) {
        node_link_t head_link = head.load(std::memory_order_acquire);
        node_link_t tail_link = tail.load(std::memory_order_acquire);
        if(head_link.ptr != tail_link.ptr) {
            node_t *node = (node_t*)head_link.ptr;
            node_link_t new_head_link = node->next;
            if(head.compare_exchange_strong(head_link,
                                            new_head_link,
                                            std::memory_order_release,
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

typedef lf_queue<int> lf_default_queue;

#endif /* LockFreeQueue_h */
