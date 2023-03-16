//
//  LockFreeStack.h
//  CppPlayground
//
//  Created by 이현우 on 2021/10/28.
//

#ifndef LockFreeStack_h
#define LockFreeStack_h

#include <atomic>
#include <memory>
#include <utility>
#include "../../Option/Option.h"

// for checking memory leaks of nodes...
#ifndef DEBUG_ALIVE_NODE_COUNT
#define DEBUG_ALIVE_NODE_COUNT 0
#endif

#if DEBUG_ALIVE_NODE_COUNT
inline static std::atomic<int32_t> debug_alive_node_count{0};
#define INC_ALIVE_NODE_COUNT debug_alive_node_count.fetch_add(1, std::memory_order_relaxed)
#define DEC_ALIVE_NODE_COUNT debug_alive_node_count.fetch_add(-1, std::memory_order_relaxed)
#define STAT_ALIVE_NODE_COUNT \
{\
int32_t val = debug_alive_node_count.load(std::memory_order_seq_cst);\
std::cout << "active node count is " << val << std::endl;\
}
#else
#define INC_ALIVE_NODE_COUNT
#define DEC_ALIVE_NODE_COUNT
#define STAT_ALIVE_NODE_COUNT
#endif

// Lock-free stack
template<typename T>
class lf_stack {
public:
    // constructor
    lf_stack() {
        head = node_link_t();
    }
    
    // destructor
    ~lf_stack() {
        T val;
        while(pop(val));
    }
    
    void push(const T &value) {
        node_t *n = new node_t;
        n->value = value;
        
        node_link_t link;
        link.ptr = (uintptr_t)n;
        link.counter = 1;
        n->next = head.load();
        while(!head.compare_exchange_strong(n->next, link));
    }

    bool pop(T &out_value) {
        node_link_t link;
        node_link_t prev_head = head.load();
        
        for(;;) {
            do {
                link = prev_head;
                link.counter++;
            }
            while(!head.compare_exchange_strong(prev_head, link));
            
            if(prev_head.ptr == 0) {
                return false;
            }
            node_t *node = (node_t*)link.ptr;
            if(head.compare_exchange_strong(link, node->next)) {
                out_value = node->value;
                int32_t ref_diff = static_cast<int32_t>(link.counter) - 2;
                if(node->ref_count.fetch_add(ref_diff) == -ref_diff) {
                    delete node;
                }
                return true;
            }
            else if(node->ref_count.fetch_sub(1) == 1) {
                delete node;
            }
        }
    }
    
    // debug-only fetch function (not thread-safe)
    void debug_fetch(T *values, uintptr_t *counters, size_t length) {
        node_link_t link = head.load();
        node_t *node = (node_t*)link.ptr;
        int index = 0;
        while(node != nullptr) {
            values[index] = node->value;
            counters[index] = 0;
            index += 1;
            node = (node_t*)node->next.ptr;
        }
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
    
    // internal node structure
    struct node_t {
        node_link_t next;
        std::atomic<int32_t> ref_count;
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
    
private:
    // top head
    alignas(64) std::atomic<node_link_t> head;
};

// default types
typedef lf_stack<int> lf_default_stack;

#endif /* LockFreeStack_h */
