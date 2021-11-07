//
//  MemoryPool.h
//  CppPlayground
//
//  Created by 이현우 on 2021/11/07.
//

#ifndef MemoryPool_h
#define MemoryPool_h

#include "../LockFree/Mutex.h"

constexpr size_t PAGE_SIZE_16KB = 16 * 1024;
constexpr size_t PAGE_SIZE_32KB = 32 * 1024;
constexpr size_t PAGE_SIZE_64KB = 64 * 1024;
constexpr size_t PAGE_SIZE_128KB = 128 * 1024;
constexpr size_t PAGE_SIZE_256KB = 256 * 1024;
constexpr size_t PAGE_SIZE_512KB = 512 * 1024;
constexpr size_t PAGE_SIZE_1MB = 1 * 1024 * 1024;
constexpr size_t PAGE_SIZE_2MB = 2 * 1024 * 1024;
constexpr size_t PAGE_SIZE_4MB = 4 * 1024 * 1024;

template<size_t block_size, size_t page_size>
class memory_pool {
public:
    memory_pool() {}
    ~memory_pool() {}
    
    void* allocate() {
        // todo
    }
    
    void free(void* ptr) {
        // todo
    }
    
private:
    
private:
    // limitations
    static_assert((block_size & (block_size - 1)) == 0, "The block size must be power of 2!");
    static_assert((page_size & (page_size - 1)) == 0, "The page size must be power of 2!");
    static_assert(block_size <= page_size, "The page size must be equal or larger than the block size!");
};

inline memory_pool<64, PAGE_SIZE_16KB>  global_memory_pool;

#endif /* MemoryPool_h */
