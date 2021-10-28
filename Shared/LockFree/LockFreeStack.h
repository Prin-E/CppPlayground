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
    
    linked_list_node() {}
    linked_list_node(const T &v) : value(v) {}
    linked_list_node(T &&v) : value(std::move(v)) {}
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
        node_t *n = new node_t(value);
        node_link_t link;
        link.ptr = (uintptr_t)n;
        do {
            n->next = head.load(std::memory_order_relaxed);
            link.counter = n->next.counter + 1;
        }
        while(!head.compare_exchange_strong(n->next, link));
    }

    bool pop(T &out_value) {
        node_link_t link, next_link;
        node_t *node = nullptr;
        do {
            link = head.load();
            node = (node_t*)link.ptr;
            std::atomic_thread_fence(std::memory_order_seq_cst);
            if(node == nullptr)
                break;
            next_link = node->next;
            next_link.counter = link.counter + 1;
        }
        while(!head.compare_exchange_weak(link, next_link));
        
        bool flag = node != nullptr;
        if(flag) {
            out_value = std::move(node->value);
            delete node;
        }
        return flag;
    }
    
    // debug-only fetch function (not thread-safe)
    void debug_fetch(T *values, uintptr_t *counters, size_t length) {
        node_link_t link = head.load(std::memory_order_seq_cst);
        int index = 0;
        while(link.ptr != 0) {
            values[index] = ((node_t*)link.ptr)->value;
            counters[index] = link.counter;
            index += 1;
            link = ((node_t*)link.ptr)->next;
        }
    }
    
private:
    std::atomic<node_link_t> head;
};

// default types
typedef lf_stack<int> lf_default_stack;

#endif /* LockFreeStack_h */
