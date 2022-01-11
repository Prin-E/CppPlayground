//
//  Atomic.h
//  CppPlayground
//
//  Created by 이현우 on 2021/11/14.
//

#pragma once

#include "PlatformDefine.h"
#include "PlatformCommon.h"
#include "../../Option/Option.h"

#if USE_MEMORY_POOL
#include "../../Shared/Memory/MemoryPool.h"
#endif

NAMESPACE_PLATFORM_BEGIN

#include <stdint.h>
#include <libkern/OSAtomic.h>

// lock-free stack
template<typename T>
class platform_lf_stack {
public:
    platform_lf_stack() : head(OS_ATOMIC_QUEUE_INIT) {
    }
    
    ~platform_lf_stack() {}
    
public:
    void push(const T &value) {
#if USE_MEMORY_POOL
        platform_node_t *n = new (global_memory_pool.allocate(sizeof(platform_node_t))) platform_node_t(value);
#else
        platform_node_t *n = new platform_node_t(value);
#endif
        OSAtomicEnqueue(&head, (void*)n, offsetof(platform_node_t, next));
    }
    
    bool pop(T &value) {
        platform_node_t *n = (platform_node_t*)OSAtomicDequeue(&head, offsetof(platform_node_t, next));
        if(n != nullptr) {
            value = n->value;
#if USE_MEMORY_POOL
            global_memory_pool.free(n);
#else
            delete n;
#endif
            return true;
        }
        return false;
    }
    
    void debug_fetch(T *values, uintptr_t *counters, size_t length) {
        platform_node_t *node = (platform_node_t*)head.opaque1;
        size_t index = 0;
        while(node != nullptr) {
            if(index >= length) {
                break;
            }
            values[index] = node->value;
            counters[index] = 0;
            index++;
            node = node->next;
        }
        
    }
    
private:
    struct platform_node_t {
        T value;
        struct platform_node_t *next;
        
        platform_node_t(const T &in_value) : value(in_value), next(nullptr) {}
        ~platform_node_t() {}
    };
    
    OSQueueHead head;
};

NAMESPACE_PLATFORM_END
