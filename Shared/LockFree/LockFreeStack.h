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
        delete_list = node_link_t();
        pop_count = 0;
    }
    
    // destructor
    ~lf_stack() {
        T val;
        while(pop(val));
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
            uintptr_t node_ptr = prev_head.ptr;
            prev_node = (node_t*)node_ptr;
            link.ptr = (uintptr_t)prev_node->next;
            link.counter = prev_head.counter + 1;
        }
        while(!head.compare_exchange_strong(prev_head, link));
        
        if(prev_node != nullptr) {
            out_value = prev_node->value;
            deferred_delete(prev_node);
            return true;
        }
        return false;
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
    // internal node structure
    struct node_t {
        struct node_t *next;
        T value;
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
    
    void deferred_delete(node_t *node) {
        if(pop_count.fetch_add(-1) == 1) {
            // delete the node immediately (+deferred list)
            node_link_t delete_link, empty_link;
            delete_link = delete_list.exchange(empty_link);
            delete_nodes_in_link(delete_link);
#if USE_MEMORY_POOL
            node->~node_t();
            global_memory_pool.free(node);
#else
            delete node;
#endif
        }
        else {
            // push the node into deferred list
            node_link_t prev_delete_link, new_delete_link;
            do {
                prev_delete_link = delete_list.load();
                new_delete_link.ptr = (uintptr_t)node;
                node->next = (node_t*)prev_delete_link.ptr;
                new_delete_link.ptr = (uintptr_t)node;
            }
            while(!delete_list.compare_exchange_strong(prev_delete_link, new_delete_link));
        }
    }
    
    void delete_nodes_in_link(node_link_t delete_link) {
        node_t *node = (node_t*)delete_link.ptr;
        while(node != nullptr) {
#if USE_MEMORY_POOL
            node->~node_t();
            global_memory_pool.free(node);
#else
            delete node;
#endif
            node = node->next;
        }
    }
    
private:
    // top head
    alignas(64) std::atomic<node_link_t> head;
    alignas(64) std::atomic<node_link_t> delete_list;
    alignas(64) std::atomic<uint64_t> pop_count;
};

// default types
typedef lf_stack<int> lf_default_stack;

#endif /* LockFreeStack_h */
