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
    struct linked_list_node *next;
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
    
    // constructor
    lf_stack() : head(nullptr) {}
    
    // destructor
    ~lf_stack() {
    }
    
    void push(const T &value) {
        node_t *n = new node_t(value);
        do {
            n->next = head.load(std::memory_order_relaxed);
        }
        while(!head.compare_exchange_strong(&n->next, n));
    }

    T pop() {
        node_t *n = nullptr;
        do {
            n = head.load(std::memory_order_relaxed);
        }
        while(n != nullptr && head.compare_exchange_strong(&n, n->next));
        
        T v = std::move(n->value);
        delete n;
        return v;
    }
    
private:
    std::atomic<node_t*> head;
};

// default types
typedef lf_stack<int> lf_default_stack;

#endif /* LockFreeStack_h */
