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
    lf_stack() {}
    
    // destructor
    ~lf_stack() {
        T val;
        while(pop(val));
        deferred_delete();
    }
    
    void push(const T &value) {
#if USE_MEMORY_POOL
        node_t *n = new (global_memory_pool.allocate()) node_t();
#else
        node_t *n = new node_t();
#endif
        n->value = value;
        node_link_t link;
        node_link_t prev_head;
        do {
            prev_head = head.load();
            link.ptr = (uintptr_t)n;
            link.counter = prev_head.counter + 1;
            n->next = (node_t*)prev_head.ptr;
        }
        while(!head.compare_exchange_strong(prev_head, link));
    }

    bool pop(T &out_value) {
        node_link_t link;
        node_link_t prev_head;
        node_t *prev_node;
        do {
            prev_head = head.load();
            if(prev_head.ptr == 0)
                return false;
            prev_node = (node_t*)prev_head.ptr;
            prev_node->internal_count.fetch_add(1);
            uintptr_t next_ptr = (uintptr_t)prev_node->next;
            link.ptr = next_ptr;
            link.counter = prev_head.counter + 1;
            if(head.compare_exchange_strong(prev_head, link)) {
                break;
            }
            else {
                prev_node->internal_count.fetch_add(-1);
            }
        }
        while(true);
        
        out_value = prev_node->value;
        if(prev_node->internal_count.fetch_add(-1) == 1) {
#if USE_MEMORY_POOL
            global_memory_pool.free(prev_node);
#else
            delete prev_node;
#endif
        }
        else {
            // other threads are referencing this node. delete it later.
            add_node_in_deferred_delete_list(prev_node);
        }
        deferred_delete();
        return true;
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
            node = node->next;
        }
    }

private:
    void deferred_delete() {
        node_link_t empty_link;
        node_link_t prev_head = deferred_delete_head.exchange(empty_link);
        node_t *node = (node_t*)prev_head.ptr;
        while(node != nullptr) {
            if(node->internal_count.load() != 0) {
                // this node is still referenced by other threads. delete it later.
                add_node_in_deferred_delete_list(node);
                break;
            }
            else {
                node_t *next_node = node->next;
#if USE_MEMORY_POOL
                global_memory_pool.free(node);
#else
                delete node;
#endif
                node = next_node;
            }
        }
    }

    void add_node_in_deferred_delete_list(void *node) {
        node_link_t prev_head, link;
        do {
            prev_head = deferred_delete_head.load();
            link.ptr = (uintptr_t)node;
            ((node_t*)node)->next = (node_t*)prev_head.ptr;
        }
        while(!deferred_delete_head.compare_exchange_strong(prev_head, link));
    }
    
private:
    // internal node structure
    struct node_t {
        struct node_t *next;
        std::atomic<int> internal_count;
        T value;
#if DEBUG_ALIVE_NODE_COUNT
        node_t() { INC_ALIVE_NODE_COUNT; }
        ~node_t() { DEC_ALIVE_NODE_COUNT; }
#endif
    };
    
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
    
    // top head
    std::atomic<node_link_t> head;
    std::atomic<node_link_t> deferred_delete_head;
};

// default types
typedef lf_stack<int> lf_default_stack;

#endif /* LockFreeStack_h */
