//
//  Option.h
//  CppPlayground
//
//  Created by 이현우 on 2021/11/14.
//

#ifndef Option_h
#define Option_h

// Whether to use memory pool instead of OS memory allocator
#ifndef USE_MEMORY_POOL
#define USE_MEMORY_POOL 0
#endif

// for checking memory leaks of nodes...
#ifndef DEBUG_ALIVE_NODE_COUNT
#define DEBUG_ALIVE_NODE_COUNT 1
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

#endif /* Option_h */
