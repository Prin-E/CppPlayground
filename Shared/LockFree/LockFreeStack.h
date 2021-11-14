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

// Linked list node
template<typename T>
struct linked_list_node {
    // node link structure
    struct node_link {
        union {
            uintptr_t value;
            struct {
                // assumes that the 64-bit system has 52-bit or lower address space.
                uintptr_t ptr : 52;
                // counter variable to prevent ABA problem (0~4096)
                uintptr_t counter : 12;
            };
        };
        node_link() : value(0) {}
        node_link(struct linked_list_node* node) : ptr((uintptr_t)node) { counter += 1; }
        operator struct linked_list_node*() const {
            return (linked_list_node*)ptr;
        }
    };

    static_assert(sizeof(node_link) == sizeof(uintptr_t), "Invalid data size of node_link!");

    node_link next;
    //struct linked_list_node *next;
    T value;
    
    linked_list_node() { INC_ALIVE_NODE_COUNT; }
    linked_list_node(const T &v) : value(v) { INC_ALIVE_NODE_COUNT; }
    linked_list_node(T &&v) : value(std::move(v)) { INC_ALIVE_NODE_COUNT; }
    ~linked_list_node() { DEC_ALIVE_NODE_COUNT; }
};

// Lock-free stack
template<typename T>
class lf_stack {
public:
    using node_t = linked_list_node<T>;
    using node_link_t = typename linked_list_node<T>::node_link;
    
    // constructor
    lf_stack() : head(nullptr) {}
    
    // destructor
    ~lf_stack() {
        T val;
        while(pop(val));
    }
    
    void push(const T &value) {
#if USE_MEMORY_POOL
        node_t *n = new (global_memory_pool.allocate()) node_t(value);
#else
        node_t *n = new node_t(value);
#endif
        node_link_t link;
        link.ptr = (uintptr_t)n;
        do {
            n->next = head.load(std::memory_order_relaxed);
            link.counter = n->next.counter + 1;
        }
        while(!head.compare_exchange_weak(n->next, link));
    }

    bool pop(T &out_value) {
        node_link_t link, next_link;
        node_t *node = nullptr;
        pop_count.fetch_add(1, std::memory_order_relaxed);
        do {
            link = head.load(std::memory_order_relaxed);
            node = (node_t*)link.ptr;
            if(node == nullptr) {
                pop_count.fetch_add(-1, std::memory_order_relaxed);
                return false;
            }
            next_link = node->next;
            next_link.counter = link.counter + 1;
        }
        while(!head.compare_exchange_weak(link, next_link));
        
        out_value = std::move(node->value);
        deferred_delete(node);
        return true;
    }
    
    // debug-only fetch function (not thread-safe)
    void debug_fetch(T *values, uintptr_t *counters, size_t length) {
        node_link_t link = head.load();
        int index = 0;
        while(link.ptr != 0) {
            values[index] = ((node_t*)link.ptr)->value;
            counters[index] = link.counter;
            index += 1;
            link = ((node_t*)link.ptr)->next;
        }
    }
    
private:
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
                prev_delete_link = delete_list.load(std::memory_order_relaxed);
                node->next = prev_delete_link;
                new_delete_link.ptr = (uintptr_t)node;
            }
            while(!delete_list.compare_exchange_weak(prev_delete_link, new_delete_link));
        }
    }
    
    void delete_nodes_in_link(node_link_t delete_link) {
        while(delete_link.ptr != 0) {
            node_t *node = (node_t*)delete_link.ptr;
            delete_link = node->next;
#if USE_MEMORY_POOL
            node->~node_t();
            global_memory_pool.free(node);
#else
            delete node;
#endif
        }
    }
    
private:
    std::atomic<node_link_t> head;
    std::atomic<node_link_t> delete_list;
    std::atomic<int> pop_count;
};

// default types
typedef lf_stack<int> lf_default_stack;

#endif /* LockFreeStack_h */
