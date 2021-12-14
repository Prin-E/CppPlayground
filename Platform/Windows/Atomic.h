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
#include <Windows.h>

// lock-free stack
template<typename T>
class platform_lf_stack {
public:
    platform_lf_stack() {
        InitializeSListHead(&head);
    }
    
    ~platform_lf_stack() {
        InterlockedFlushSList(&head);
    }
    
public:
    void push(const T &value) {
#if USE_MEMORY_POOL
        // global memory pool is 16-byte or higher aligned.
        platform_node_t *n = new (global_memory_pool.allocate()) platform_node_t(value);
#else
        platform_node_t *n = new (_aligned_malloc(sizeof(platform_node_t), MEMORY_ALLOCATION_ALIGNMENT)) platform_node_t(value);
#endif
        InterlockedPushEntrySList(&head, &n->entry);
    }
    
    bool pop(T &value) {
        PSLIST_ENTRY entry = InterlockedPopEntrySList(&head);
        platform_node_t *n = (platform_node_t*)entry;
        if(n != nullptr) {
            value = n->value;
#if USE_MEMORY_POOL
            global_memory_pool.free(n);
#else
            _aligned_free(n);
#endif
            return true;
        }
        return false;
    }
    
    void debug_fetch(T *values, uintptr_t *counters, size_t length) {
        platform_node_t *node = (platform_node_t*)head.HeaderX64.NextEntry;
        size_t index = 0;
        while(node != nullptr) {
            if(index >= length) {
                break;
            }
            values[index] = node->value;
            counters[index] = 0;
            index++;
            node = (platform_node_t*)node->entry;
        }
        
    }
    
private:
    struct platform_node_t {
        SLIST_ENTRY entry;
        T value;
        
        platform_node_t(const T &in_value) : value(in_value) {}
        ~platform_node_t() {}
    };
    
    alignas(MEMORY_ALLOCATION_ALIGNMENT) SLIST_HEADER head;
};

NAMESPACE_PLATFORM_END
